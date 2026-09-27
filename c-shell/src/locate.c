#include "locate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>

/*This helper function takes a file path string as input and determines whether the path points 
to a valid executable file. It uses the stat system call to verify that the target path exists 
and refers to a regular file or symbolic link, and then uses the access function with the X_OK 
flag to verify that the current user has execution permissions for that file. It returns 1 if 
all checks pass successfully, or 0 if the file does not exist, is a directory, or lacks execution 
permissions.*/
static int is_executable(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0 && access(path, X_OK) == 0) {
        if (S_ISREG(st.st_mode)) {
            return 1;
        }
    }
    return 0;
}

/*This helper function constructs a full file path by combining a directory path and a command 
name string into a single path buffer. Once constructed, it calls the is_executable function to 
check if the combined path points to a valid executable binary, and if so, it uses the realpath 
function to resolve the true absolute file path from relative components before printing that 
absolute path to standard output. It returns 1 if a matching executable was successfully located 
and printed, or 0 if no valid executable was found at that constructed path location.*/
static int check_and_print_path(const char *dir, const char *cmd) {
    char full_path[PATH_MAX];

    if (snprintf(full_path, sizeof(full_path), "%s/%s", dir, cmd) >= (int)sizeof(full_path)) {
        return 0;
    }

    if (is_executable(full_path)) {
        char resolved_path[PATH_MAX];
        if (realpath(full_path, resolved_path) != NULL) {
            printf("%s\n", resolved_path);
            return 1;
        }
    }
    return 0;
}

/*This function performs the core path resolution search for a single command name argument. It first 
checks the current working directory by passing dot as the directory parameter to check_and_print_path,
and then retrieves the system PATH environment variable string, makes a mutable copy of it, and parses 
each colon-separated directory entry in sequence using strtok to search every PATH directory in order. 
If a matching executable is found in either the current directory or any PATH directory, all corresponding 
absolute paths are printed in search order, but if no matching executable is located anywhere, it prints 
an error message stating that the command was not found.*/
static void locate_single_command(const char *cmd) {
    int found_any = 0;

    if (check_and_print_path(".", cmd)) {
        found_any = 1;
    }

    char *path_env = getenv("PATH");
    if (path_env != NULL) {
        char *path_copy = strdup(path_env);
        if (path_copy != NULL) {
            char *dir = strtok(path_copy, ":");
            while (dir != NULL) {
                const char *target_dir = (strlen(dir) == 0) ? "." : dir;
                
                if (check_and_print_path(target_dir, cmd)) {
                    found_any = 1;
                }
                dir = strtok(NULL, ":");
            }
            free(path_copy);
        }
    }

    if (!found_any) {
        printf("locate: command not found (%s)\n", cmd);
    }
}

/*This is the primary execution function called when the user enters the locate command in the shell 
interface. It first checks if the argument count is zero and immediately prints an invalid syntax 
error message if no command names were supplied. Otherwise, it iterates through every argument passed 
in the array in the order specified by the user, delegating each command name to the locate_single_command 
function to process each target independently.*/
void execute_locate(char **args, int arg_count) {
    if (arg_count == 0) {
        printf("locate: invalid syntax\n");
        return;
    }

    for (int i = 0; i < arg_count; i++) {
        locate_single_command(args[i]);
    }
}