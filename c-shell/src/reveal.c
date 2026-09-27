#include "reveal.h"
#include "prompt.h"
#include "hop.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>

extern char prev_working_dir[PATH_MAX];

/*Compares two directory entry names alphabetically.
This function is used by qsort() to arrange the files and directories
in alphabetical order before displaying them.*/
static int compare_strings(const void *a, const void *b) {
    const char *str_a = *(const char **)a;
    const char *str_b = *(const char **)b;
    return strcmp(str_a, str_b);
}

/*Displays a file or directory in long format.
It obtains information such as file type, permissions, number of links,
owner, group, size, and last modification time using stat(), and then
prints these details along with the filename.*/
static void print_long_format(const char *dir_path, const char *filename) {
    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, filename);

    struct stat st;
    if (stat(full_path, &st) != 0) {
        printf("%s\n", filename);
        return;
    }

    printf((S_ISDIR(st.st_mode)) ? "d" : "-");
    printf((st.st_mode & S_IRUSR) ? "r" : "-");
    printf((st.st_mode & S_IWUSR) ? "w" : "-");
    printf((st.st_mode & S_IXUSR) ? "x" : "-");
    printf((st.st_mode & S_IRGRP) ? "r" : "-");
    printf((st.st_mode & S_IWGRP) ? "w" : "-");
    printf((st.st_mode & S_IXGRP) ? "x" : "-");
    printf((st.st_mode & S_IROTH) ? "r" : "-");
    printf((st.st_mode & S_IWOTH) ? "w" : "-");
    printf((st.st_mode & S_IXOTH) ? "x" : "-");

    printf(" %2lu", (unsigned long)st.st_nlink);

    struct passwd *pw = getpwuid(st.st_uid);
    struct group  *gr = getgrgid(st.st_gid);
    printf(" %s %s", pw ? pw->pw_name : "user", gr ? gr->gr_name : "group");

    printf(" %5lld", (long long)st.st_size);

    char time_buf[64];
    struct tm *tm_info = localtime(&st.st_mtime);
    strftime(time_buf, sizeof(time_buf), "%b %d %H:%M", tm_info);
    printf(" %s", time_buf);

    printf(" %s\n", filename);
}

/*Reads and displays the contents of a directory.
It collects directory entries, optionally includes hidden files,
sorts them alphabetically, and displays them normally or in long format.
If recursive mode is enabled, it also enters each subdirectory and
displays its contents.*/
static void list_directory(const char *dir_path, const char *display_prefix, int show_all, int recursive, int long_format) {
    DIR *dir = opendir(dir_path);
    if (!dir) return;

    struct dirent *entry;
    char **entries = NULL;
    size_t count = 0;
    size_t capacity = 16;

    entries = malloc(capacity * sizeof(char *));
    if (!entries) {
        closedir(dir);
        return;
    }

    /*Read each entry from the directory and store its name in the
    dynamically allocated array. Hidden files are skipped unless
    show_all is enabled, and the array grows when it becomes full.*/
    while ((entry = readdir(dir)) != NULL) {
        if (!show_all && entry->d_name[0] == '.') {
            continue;
        }

        if (count >= capacity) {
            capacity *= 2;
            char **temp = realloc(entries, capacity * sizeof(char *));
            if (!temp) break;
            entries = temp;
        }

        entries[count] = strdup(entry->d_name);
        count++;
    }
    closedir(dir);

    qsort(entries, count, sizeof(char *), compare_strings);

    /*Display every collected entry. Depending on the selected options,
    the entry is printed normally or using long format. If recursive
    mode is enabled and the entry is a directory, the function calls
    itself to display that directory's contents.*/
    for (size_t i = 0; i < count; i++) {
        if (long_format) {
            print_long_format(dir_path, entries[i]);
        } else {
            char full_entry_display[PATH_MAX];
            if (strlen(display_prefix) > 0) {
                snprintf(full_entry_display, sizeof(full_entry_display), "%s/%s", display_prefix, entries[i]);
            } else {
                snprintf(full_entry_display, sizeof(full_entry_display), "%s", entries[i]);
            }
            printf("%s\n", full_entry_display);
        }

        if (recursive && strcmp(entries[i], ".") != 0 && strcmp(entries[i], "..") != 0) {
            char sub_path[PATH_MAX];
            snprintf(sub_path, sizeof(sub_path), "%s/%s", dir_path, entries[i]);

            struct stat st;
            if (stat(sub_path, &st) == 0 && S_ISDIR(st.st_mode)) {
                list_directory(sub_path, entries[i], show_all, recursive, long_format);
            }
        }

        free(entries[i]);
    }
    free(entries);
}

/*Executes the reveal command and processes its options and target directory.
The -a option shows hidden files, -t enables recursive listing, and -l
displays detailed file information. It also handles special paths such as
the current directory, home directory, parent directory, and previous
working directory before calling list_directory().*/
void execute_reveal(char **args, int arg_count) {
    int show_all = 0;
    int recursive = 0;
    char *target_arg = NULL;

    /*Process all command-line arguments and identify the supported options.
    Only one target directory is allowed, and an invalid option or multiple
    target arguments cause the command to stop with an error.*/
    for (int i = 0; i < arg_count; i++) {
        if (args[i][0] == '-' && strlen(args[i]) > 1) {
            for (size_t j = 1; j < strlen(args[i]); j++) {
                if (args[i][j] == 'a') {
                    show_all = 1;
                } else if (args[i][j] == 't') {
                    recursive = 1;
                } else {
                    printf("reveal: invalid syntax\n");
                    return;
                }
            }
        } else {
            if (target_arg != NULL) {
                printf("reveal: invalid syntax\n");
                return;
            }
            target_arg = args[i];
        }
    }

    char resolved_target[PATH_MAX];

    /*Resolve the target directory according to the argument provided.
    If no target or "." is given, the current directory is used. Special
    paths "~", "..", and "-" are handled separately before using the
    resulting path for directory listing.*/
    if (target_arg == NULL || strcmp(target_arg, ".") == 0) {
        if (getcwd(resolved_target, sizeof(resolved_target)) == NULL) {
            printf("reveal: no such directory\n");
            return;
        }
    } else if (strcmp(target_arg, "~") == 0) {
        snprintf(resolved_target, sizeof(resolved_target), "%s", getenv("HOME") ? getenv("HOME") : ".");
    } else if (strcmp(target_arg, "..") == 0) {
        snprintf(resolved_target, sizeof(resolved_target), "..");
    } else if (strcmp(target_arg, "-") == 0) {
        if (strlen(prev_working_dir) == 0) {
            printf("reveal: no such directory\n");
            return;
        }
        snprintf(resolved_target, sizeof(resolved_target), "%s", prev_working_dir);
    } else {
        snprintf(resolved_target, sizeof(resolved_target), "%s", target_arg);
    }

    /*Verify that the resolved target exists and is actually a directory.
    If it is invalid, display an error and stop; otherwise, list its
    Contents using the options selected by the user.*/
    struct stat st;
    if (stat(resolved_target, &st) != 0 || !S_ISDIR(st.st_mode)) {
        printf("reveal: no such directory\n");
        return;
    }

    list_directory(resolved_target, "", show_all, recursive, 0);
}