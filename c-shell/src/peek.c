#include "peek.h"
#include "prompt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

/* Dynamic array structure to hold lines read from a stream */
typedef struct {
    char **lines;
    size_t count;
    size_t capacity;
} LineBuffer;

/* Initialize line buffer allocation */
static void init_line_buffer(LineBuffer *lb) {
    lb->count = 0;
    lb->capacity = 16;
    lb->lines = malloc(lb->capacity * sizeof(char *));
}

/* Dynamically add a line to the buffer */
static void add_line(LineBuffer *lb, const char *line) {
    if (lb->count >= lb->capacity) {
        lb->capacity *= 2;
        lb->lines = realloc(lb->lines, lb->capacity * sizeof(char *));
    }
    lb->lines[lb->count++] = strdup(line);
}

/* Free allocated memory for the buffer */
static void free_line_buffer(LineBuffer *lb) {
    for (size_t i = 0; i < lb->count; i++) {
        free(lb->lines[i]);
    }
    free(lb->lines);
}

/* Print accumulated lines according to specified flags:
   - flag_n: prepend 1-based index to non-empty lines
   - flag_r: output lines in reverse order */
static void print_lines(LineBuffer *lb, int flag_n, int flag_r) {
    int line_num = 1;

    if (!flag_r) {
        /* Standard forward printing */
        for (size_t i = 0; i < lb->count; i++) {
            if (flag_n) {
                if (lb->lines[i][0] != '\n' && lb->lines[i][0] != '\0') {
                    printf("%d %s", line_num++, lb->lines[i]);
                } else {
                    printf("%s", lb->lines[i]);
                }
            } else {
                printf("%s", lb->lines[i]);
            }
        }
    } else {
        /* Reverse line printing */
        for (ssize_t i = (ssize_t)lb->count - 1; i >= 0; i--) {
            if (flag_n) {
                if (lb->lines[i][0] != '\n' && lb->lines[i][0] != '\0') {
                    printf("%d %s", line_num++, lb->lines[i]);
                } else {
                    printf("%s", lb->lines[i]);
                }
            } else {
                printf("%s", lb->lines[i]);
            }
        }
    }
}

/* Reads all lines from an open file pointer and triggers formatted output */
static void process_stream(FILE *fp, int flag_n, int flag_r) {
    LineBuffer lb;
    init_line_buffer(&lb);

    char *line = NULL;
    size_t len = 0;
    while (getline(&line, &len, fp) != -1) {
        add_line(&lb, line);
    }
    free(line);

    print_lines(&lb, flag_n, flag_r);
    free_line_buffer(&lb);
}

/* Resolves leading ~ to home directory */
static void resolve_path(const char *path, char *resolved_path) {
    const char *home = get_home_dir();
    if (path[0] == '~') {
        if (path[1] == '\0') {
            strcpy(resolved_path, home);
        } else if (path[1] == '/') {
            snprintf(resolved_path, 4096, "%s%s", home, path + 1);
        } else {
            strcpy(resolved_path, path);
        }
    } else {
        strcpy(resolved_path, path);
    }
}

/* Execute command logic for 'peek' */
void execute_peek(char **args, int arg_count) {
    int flag_n = 0;
    int flag_r = 0;
    char *files[256];
    int file_count = 0;

    /* Parse flags and collect target filenames */
    for (int i = 0; i < arg_count; i++) {
        if (args[i][0] == '-' && args[i][1] != '\0') {
            for (int j = 1; args[i][j] != '\0'; j++) {
                if (args[i][j] == 'n') {
                    flag_n = 1;
                } else if (args[i][j] == 'r') {
                    flag_r = 1;
                } else {
                    printf("peek: invalid syntax\n");
                    return;
                }
            }
        } else {
            files[file_count++] = args[i];
        }
    }

    /* Handle stdin input if no target files provided */
    if (file_count == 0) {
        process_stream(stdin, flag_n, flag_r);
        return;
    }

    /* Process target files sequentially */
    for (int i = 0; i < file_count; i++) {
        if (strcmp(files[i], "-") == 0) {
            process_stream(stdin, flag_n, flag_r);
            continue;
        }

        char target_path[4096];
        resolve_path(files[i], target_path);

        struct stat st;
        if (stat(target_path, &st) != 0) {
            printf("peek: no such file or directory\n");
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            printf("peek: is a directory\n");
            continue;
        }

        FILE *fp = fopen(target_path, "r");
        if (!fp) {
            printf("peek: no such file or directory\n");
            continue;
        }

        process_stream(fp, flag_n, flag_r);
        fclose(fp);
    }
}