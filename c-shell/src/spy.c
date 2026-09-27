#include "spy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>
#include <errno.h>

#define MAX_PATH 1024
#define MAX_MEM_FILES 512

/* Helper to convert file mode from stat() to standard type string */
static const char *get_file_type(const char *path) {
    struct stat st;
    if (stat(path, &st) < 0) {
        return "UNKNOWN";
    }

    if (S_ISREG(st.st_mode))  return "REG";
    if (S_ISDIR(st.st_mode))  return "DIR";
    if (S_ISCHR(st.st_mode))  return "CHR";
    if (S_ISBLK(st.st_mode))  return "BLK";
    if (S_ISFIFO(st.st_mode)) return "FIFO";
    if (S_ISSOCK(st.st_mode)) return "SOCK";
    if (S_ISLNK(st.st_mode))  return "LNK";

    return "UNKNOWN";
}

/* Helper to print formatted spy output row */
static void print_spy_entry(pid_t pid, const char *fd, const char *path) {
    const char *type = get_file_type(path);
    printf("%-6d %-6s %-6s %s\n", pid, fd, type, path);
}

/* Print cwd symbolic link */
static void inspect_cwd(pid_t pid) {
    char proc_path[MAX_PATH];
    char target_path[MAX_PATH];

    snprintf(proc_path, sizeof(proc_path), "/proc/%d/cwd", pid);
    ssize_t len = readlink(proc_path, target_path, sizeof(target_path) - 1);
    if (len != -1) {
        target_path[len] = '\0';
        print_spy_entry(pid, "cwd", target_path);
    }
}

/* Print txt (executable) symbolic link */
static void inspect_txt(pid_t pid) {
    char proc_path[MAX_PATH];
    char target_path[MAX_PATH];

    snprintf(proc_path, sizeof(proc_path), "/proc/%d/exe", pid);
    ssize_t len = readlink(proc_path, target_path, sizeof(target_path) - 1);
    if (len != -1) {
        target_path[len] = '\0';
        print_spy_entry(pid, "txt", target_path);
    }
}

/* Parse /proc/<pid>/maps and print unique memory-mapped files */
static void inspect_mem(pid_t pid) {
    char maps_path[MAX_PATH];
    snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", pid);

    FILE *fp = fopen(maps_path, "r");
    if (!fp) return;

    char line[MAX_PATH];
    char seen_paths[MAX_MEM_FILES][MAX_PATH];
    int seen_count = 0;

    while (fgets(line, sizeof(line), fp)) {
        // Find path in maps line (path starts after space/tab near end)
        char *path_start = strchr(line, '/');
        if (!path_start) continue;

        // Strip trailing newline
        path_start[strcspn(path_start, "\r\n")] = '\0';

        // Check if path was already printed
        int duplicate = 0;
        for (int i = 0; i < seen_count; i++) {
            if (strcmp(seen_paths[i], path_start) == 0) {
                duplicate = 1;
                break;
            }
        }

        if (!duplicate && seen_count < MAX_MEM_FILES) {
            strncpy(seen_paths[seen_count], path_start, MAX_PATH - 1);
            seen_paths[seen_count][MAX_PATH - 1] = '\0';
            seen_count++;

            print_spy_entry(pid, "mem", path_start);
        }
    }

    fclose(fp);
}

/* Scan /proc/<pid>/fd/ directory for numeric file descriptors */
static void inspect_fds(pid_t pid) {
    char fd_dir_path[MAX_PATH];
    snprintf(fd_dir_path, sizeof(fd_dir_path), "/proc/%d/fd", pid);

    DIR *dir = opendir(fd_dir_path);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip '.' and '..'
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char link_path[MAX_PATH];
        char target_path[MAX_PATH];

        snprintf(link_path, sizeof(link_path), "/proc/%d/fd/%s", pid, entry->d_name);
        ssize_t len = readlink(link_path, target_path, sizeof(target_path) - 1);

        if (len != -1) {
            target_path[len] = '\0';
            print_spy_entry(pid, entry->d_name, target_path);
        }
    }

    closedir(dir);
}

int handle_spy_command(char **args, int arg_count) {
    // 1. Validate argument count
    if (arg_count > 2) {
        printf("spy: invalid syntax\n");
        return 1;
    }

    pid_t target_pid;

    if (arg_count == 1) {
        target_pid = getpid();
    } else {
        // Check if argument is numeric
        for (int i = 0; args[1][i] != '\0'; i++) {
            if (!isdigit((unsigned char)args[1][i])) {
                printf("spy: invalid syntax\n");
                return 1;
            }
        }
        target_pid = (pid_t)atoi(args[1]);
    }

    // 2. Validate process existence via /proc/<pid>
    char proc_dir[MAX_PATH];
    snprintf(proc_dir, sizeof(proc_dir), "/proc/%d", target_pid);

    if (access(proc_dir, F_OK) != 0) {
        printf("spy: no such process\n");
        return 1;
    }

    // 3. Output Table Header
    printf("%-6s %-6s %-6s %s\n", "PID", "FD", "TYPE", "PATH");

    // 4. Collect and print process file details in required order
    inspect_cwd(target_pid);
    inspect_txt(target_pid);
    inspect_mem(target_pid);
    inspect_fds(target_pid);

    return 1;
}