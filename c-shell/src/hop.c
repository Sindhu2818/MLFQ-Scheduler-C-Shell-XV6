#include "hop.h"
#include "prompt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <time.h>

/* Track the shell's home directory (captured at initial boot) and the previous 
   working directory needed for the '-' argument flag. Storing them as static 
   limits their scope entirely to this source file.using static restricts it 
   to only this file */
static char shell_home_dir[PATH_MAX];
char prev_working_dir[PATH_MAX] = "";

/* A structure to store entries from our persistent frecency file.
   Each entry holds a absolute path, the total visit count, and the timestamp of last access. */
typedef struct {
    char path[PATH_MAX];
    int count;
    time_t last_visited;
} FrecencyEntry;

#define MAX_FRECENCY_ENTRIES 500
#define HISTORY_FILE_NAME ".hop_history" //hidden file in directory list 

/* Helper function to construct the full path to our hidden frecency history file 
   which is saved inside the shell's initial home directory. */
static void get_history_file_path(char *out_path, size_t size) {
    snprintf(out_path, size, "%s/%s", shell_home_dir, HISTORY_FILE_NAME);
}

/* Reads through the persistent history file and updates or appends the target directory.
   Every time we successfully change into a directory, this function increments its visit count 
   and updates its timestamp so recency is preserved across shell sessions. */
static void record_directory_visit(const char *target_path) {
    // Resolve the directory to an absolute canonical path first
    char resolved_path[PATH_MAX];
    if (realpath(target_path, resolved_path) == NULL) {
        // If realpath fails (e.g. invalid permissions), fall back to raw string copy
        strncpy(resolved_path, target_path, sizeof(resolved_path) - 1);
        resolved_path[sizeof(resolved_path) - 1] = '\0';
    }

    char history_path[PATH_MAX];
    get_history_file_path(history_path, sizeof(history_path));

    FrecencyEntry entries[MAX_FRECENCY_ENTRIES];
    int count = 0;
    int found = 0;

    // Open file for reading existing records
    FILE *file = fopen(history_path, "r");
    if (file) {
        while (count < MAX_FRECENCY_ENTRIES && 
               fscanf(file, "%s %d %ld", entries[count].path, &entries[count].count, &entries[count].last_visited) == 3) {
            
            // Check if directory already exists in our database
            if (strcmp(entries[count].path, resolved_path) == 0) {
                entries[count].count += 1;                  // Increment visit count (Frequency)
                entries[count].last_visited = time(NULL);   // Update access timestamp (Recency)
                found = 1;
            }
            count++;
        }
        fclose(file);
    }

    // If this is a new directory, append it to the record list
    if (!found && count < MAX_FRECENCY_ENTRIES) {
        strncpy(entries[count].path, resolved_path, sizeof(entries[count].path) - 1);
        entries[count].path[sizeof(entries[count].path) - 1] = '\0';
        entries[count].count = 1;
        entries[count].last_visited = time(NULL);
        count++;
    }

    // Write all updated entries back out to the history file
    file = fopen(history_path, "w");
    if (file) {
        for (int i = 0; i < count; i++) {
            fprintf(file, "%s %d %ld\n", entries[i].path, entries[i].count, entries[i].last_visited);
        }
        fclose(file);
    }
}

/* Performs a frecency search when a raw directory name fails to resolve directly.
   It looks up all stored paths containing 'search_str' as a substring, calculates 
   a frecency score = (count / time_elapsed), and returns the highest-scoring valid path. */
static int find_frecency_match(const char *search_str, char *out_matched_path) {
    char history_path[PATH_MAX];
    get_history_file_path(history_path, sizeof(history_path));

    FILE *file = fopen(history_path, "r");
    if (!file) return 0; // History file does not exist yet

    FrecencyEntry current;
    double highest_score = -1.0;
    int match_found = 0;
    time_t now = time(NULL);

    while (fscanf(file, "%s %d %ld", current.path, &current.count, &current.last_visited) == 3) {
        // Check if the target query string exists anywhere as a substring inside the recorded path
        if (strstr(current.path, search_str) != NULL) {
            
            // Verify that the directory actually exists on disk right now
            struct stat st;
            if (stat(current.path, &st) == 0 && S_ISDIR(st.st_mode)) {
                
                // Calculate frecency score: frequency weight penalized by elapsed seconds (recency)
                double seconds_elapsed = difftime(now, current.last_visited);
                if (seconds_elapsed < 1.0) seconds_elapsed = 1.0; // Avoid division by zero
                
                double score = (double)current.count / (seconds_elapsed / 3600.0); // Visits per hour ratio

                if (score > highest_score) {
                    highest_score = score;
                    strncpy(out_matched_path, current.path, PATH_MAX - 1);
                    out_matched_path[PATH_MAX - 1] = '\0';
                    match_found = 1;
                }
            }
        }
    }

    fclose(file);
    return match_found;
}

/* Internal helper function to safely change directory using system call chdir().
   Before executing the jump, it saves the current directory into 'prev_working_dir' 
   so the '-' argument flag can properly revert to it later. */
static int perform_chdir(const char *target_path) {
    char current_cwd[PATH_MAX];
    if (getcwd(current_cwd, sizeof(current_cwd)) == NULL) {
        return -1;
    }

    // Execute system call to change directory
    if (chdir(target_path) == 0) {
        // Update our previous working directory tracking variable on successful jump
        strncpy(prev_working_dir, current_cwd, sizeof(prev_working_dir) - 1);
        prev_working_dir[sizeof(prev_working_dir) - 1] = '\0';

        // Log this successful jump into our persistent frecency database
        record_directory_visit(target_path);
        return 0;
    }

    return -1;
}

/* Initialization function called when the shell starts up to register the root home folder. */
void init_hop(void) {
    if (getcwd(shell_home_dir, sizeof(shell_home_dir)) == NULL) {
        perror("getcwd failed during hop initialization");
    }
}

/* Core execution function for 'hop'. It parses arguments sequentially from left to right.
   For each token:
     - Handles '~' or NULL (Home)
     - Handles '.' (Do nothing)
     - Handles '..' (Parent directory)
     - Handles '-' (Previous directory)
     - Tries direct path lookup
     - Falls back to frecency match lookup if direct lookup fails */
void execute_hop(char **args, int arg_count) {
    // Case 1: 'hop' called with zero arguments -> Default jump to shell home directory
    if (arg_count == 0) {
        if (perform_chdir(shell_home_dir) != 0) {
            printf("hop: no such directory\n");
        }
        return;
    }

    // Process all passed arguments sequentially (e.g. 'hop dir1 .. -')
    for (int i = 0; i < arg_count; i++) {
        char *target = args[i];

        // Sub-case A: "~" -> Jump to shell home directory
        if (strcmp(target, "~") == 0) {
            if (perform_chdir(shell_home_dir) != 0) {
                printf("hop: no such directory\n");
            }
        }
        // Sub-case B: "." -> Do nothing (stay in current directory)
        else if (strcmp(target, ".") == 0) {
            continue;
        }
        // Sub-case C: ".." -> Jump to parent directory
        else if (strcmp(target, "..") == 0) {
            if (perform_chdir("..") != 0) {
                printf("hop: no such directory\n");
            }
        }
        // Sub-case D: "-" -> Jump to previous working directory
        else if (strcmp(target, "-") == 0) {
            if (strlen(prev_working_dir) == 0) {
                // If no previous directory has been recorded yet, do nothing
                continue;
            }
            if (perform_chdir(prev_working_dir) != 0) {
                printf("hop: no such directory\n");
            }
        }
        // Sub-case E: Direct Path OR Frecency Fallback
        else {
            // 1. First try changing to direct relative or absolute path
            if (perform_chdir(target) == 0) {
                continue;
            }

            // 2. If direct path fails, attempt Frecency lookup
            char frecency_matched_path[PATH_MAX];
            if (find_frecency_match(target, frecency_matched_path)) {
                if (perform_chdir(frecency_matched_path) == 0) {
                    continue;
                }
            }

            // 3. Both direct and frecency lookups failed
            printf("hop: no such directory\n");
        }
    }
}