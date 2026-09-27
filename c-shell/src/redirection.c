#include "redirection.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

int process_input_redirection(Token *tokens, char **clean_args, int *clean_arg_count) {
    /*This function processes input and output redirection in the command.
    It identifies <, > and >> operators, separates them from the normal
    command arguments, checks the required files, and redirects standard
    input or output using file descriptors.*/
    *clean_arg_count = 0;

    char *input_files[256];
    int input_count = 0;

    Token *curr = tokens;

    /*Traverse the token list and identify redirection operators.
    Normal words are stored in clean_args, while redirection operators
    and their filenames are processed separately.*/
    while (curr != NULL) {
        if ((curr->type == TOKEN_WORD && strcmp(curr->value, "<") == 0) || curr->type == TOKEN_LT) {
            curr = curr->next;
            if (curr != NULL && curr->type == TOKEN_WORD) {
                input_files[input_count++] = curr->value;
            } else {
                printf("cshell: invalid syntax\n");
                return -1;
            }
        } 
        else if ((curr->type == TOKEN_WORD && strcmp(curr->value, ">") == 0) || curr->type == TOKEN_GT) {
            curr = curr->next;
            if (curr != NULL && curr->type == TOKEN_WORD) {
                int fd = open(curr->value, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) {
                    perror("cshell: open");
                    return -1;
                }
                if (dup2(fd, STDOUT_FILENO) < 0) {
                    perror("cshell: dup2");
                    close(fd);
                    return -1;
                }
                close(fd);
            } else {
                printf("cshell: invalid syntax\n");
                return -1;
            }
        } 
        else if ((curr->type == TOKEN_WORD && strcmp(curr->value, ">>") == 0) || curr->type == TOKEN_GTGT) {
            curr = curr->next;
            if (curr != NULL && curr->type == TOKEN_WORD) {
                int fd = open(curr->value, O_WRONLY | O_CREAT | O_APPEND, 0644);
                if (fd < 0) {
                    perror("cshell: open");
                    return -1;
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            } else {
                printf("cshell: invalid syntax\n");
                return -1;
            }
        } 
        else if (curr->type == TOKEN_WORD) {
            clean_args[(*clean_arg_count)++] = curr->value;
        }

        curr = curr->next;
    }

    clean_args[*clean_arg_count] = NULL;

    /*If input redirection is present, open every input file in read-only
    mode. If any file cannot be opened, close the files already opened
    and stop execution with an error.*/
    if (input_count > 0) {
        int fds[256];
        for (int i = 0; i < input_count; i++) {
            int fd = open(input_files[i], O_RDONLY);
            if (fd < 0) {
                printf("cshell: no such file or directory\n");
                for (int j = 0; j < i; j++) {
                    close(fds[j]);
                }
                return -1;
            }
            fds[i] = fd;
        }

        /*For a single input file, directly connect the file to standard
        input. For multiple files, combine their contents into a temporary
        file in the given order and redirect standard input to it.*/
        if (input_count == 1) {
            if (dup2(fds[0], STDIN_FILENO) < 0) {
                perror("dup2");
                close(fds[0]);
                return -1;
            }
            close(fds[0]);
        } else {
            FILE *temp_fp = tmpfile();
            if (temp_fp == NULL) {
                perror("tmpfile");
                for (int i = 0; i < input_count; i++) close(fds[i]);
                return -1;
            }

            char buffer[4096];

            /*Read each input file and write its contents into the temporary
            file sequentially so that the final input contains all files
            in the same order in which they were specified.*/
            for (int i = 0; i < input_count; i++) {
                ssize_t bytes_read;
                while ((bytes_read = read(fds[i], buffer, sizeof(buffer))) > 0) {
                    fwrite(buffer, 1, bytes_read, temp_fp);
                }
                close(fds[i]);
            }

            fflush(temp_fp);
            rewind(temp_fp);

            int temp_fd = fileno(temp_fp);
            if (dup2(temp_fd, STDIN_FILENO) < 0) {
                perror("dup2");
                fclose(temp_fp);
                return -1;
            }
        }
    }

    return 0;
}