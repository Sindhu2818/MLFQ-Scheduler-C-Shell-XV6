#include "execute.h"
#include "redirection.h"
#include "background.h"
#include "activities.h"
#include "terminal_control.h"
#include "resume.h"
#include "ping.h"
#include "spy.h"
#include "snoop.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <limits.h>
#include <fcntl.h>
#include <signal.h>

/*This helper function takes a file path string as input and checks whether the path corresponds 
to a valid regular file with execution permissions. It calls stat to verify that the file exists 
and is a regular file or symbolic link, and then invokes access with the X_OK flag to verify 
execution rights, returning 1 if the path is a valid executable or 0 if it does not exist, is a 
directory, or lacks execution permissions.*/
static int is_executable(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode) && access(path, X_OK) == 0) {
        return 1; 
    }
    return 0;
}

/*This helper function attempts to locate an executable binary inside a specified target directory 
for a given command string. It constructs a combined full path string by joining the directory name 
and command name, checks if the resulting file is executable via is_executable, and copies the 
resolved absolute path into the output path buffer using realpath while returning 1 on success 
or 0 if no valid binary exists at that path.*/
static int check_dir_for_exec(const char *dir, const char *cmd, char *out_path) {
    char full_path[PATH_MAX];
    if (snprintf(full_path, sizeof(full_path), "%s/%s", dir, cmd) >= (int)sizeof(full_path)) {
        return 0;
    }
    if (is_executable(full_path)) {
        if (realpath(full_path, out_path) != NULL) {
            return 1;
        }
        strncpy(out_path, full_path, PATH_MAX - 1);
        out_path[PATH_MAX - 1] = '\0';
        return 1;
    }
    return 0;
}

/*This function resolves the exact path of an external command according to the assignment requirement 
hierarchy. If the command name contains a slash character, it treats it as a literal path and returns 
success if executable; if it starts with a percent sign, it strips the percent prefix and searches 
only the PATH environment variable directories; otherwise, it checks the current working directory 
first before falling back to searching PATH directories in order, returning 1 if a matching binary 
path was successfully resolved or 0 if no executable binary was found.*/
static int resolve_command_path(const char *cmd_name, char *resolved_path) {
    // If command name contains '/', treat as a literal path directly
    if (strchr(cmd_name, '/') != NULL) {
        if (is_executable(cmd_name)) {
            strncpy(resolved_path, cmd_name, PATH_MAX - 1);
            resolved_path[PATH_MAX - 1] = '\0';
            return 1;
        }
        return 0;
    }

    // Override Path (% prefix): bypass CWD, search PATH directly using actual_cmd
    const char *actual_cmd = cmd_name;
    int skip_cwd = 0;
    if (cmd_name[0] == '%') {
        actual_cmd = cmd_name + 1;
        skip_cwd = 1;
    }

    // Search CWD if not skipped
    if (!skip_cwd && check_dir_for_exec(".", actual_cmd, resolved_path)) {
        return 1;
    }

    // Search PATH directories in order
    char *path_env = getenv("PATH");
    if (path_env != NULL) {
        char *path_copy = strdup(path_env);
        if (path_copy != NULL) {
            char *dir = strtok(path_copy, ":");
            while (dir != NULL) {
                const char *target_dir = (strlen(dir) == 0) ? "." : dir;
                if (check_dir_for_exec(target_dir, actual_cmd, resolved_path)) {
                    free(path_copy);
                    return 1;
                }
                dir = strtok(NULL, ":");
            }
            free(path_copy);
        }
    }

    return 0;
}

/*This function executes external commands with full terminal control and background management support.
 It constructs token linked lists, forks child processes, and assigns process group IDs via setpgid.
 For foreground processes, terminal control is passed via give_terminal_to_pgid and signal actions for SIGINT/SIGTSTP 
 are restored to standard behavior in the child process. The parent shell waits using WUNTRACED to detect stopped commands 
 (e.g. Ctrl-Z), updates the job tracker accordingly, and reclaims terminal ownership using reclaim_terminal upon completion.*/
int execute_external_command_bg(char **args, int arg_count, int is_background) {
    if (arg_count == 0 || args[0] == NULL) {
        return 1;
    }

    if (strcmp(args[0], "resume") == 0) {
        return handle_resume_command(args, arg_count);
    }

    // Intercept 'ping' built-in
    if (strcmp(args[0], "ping") == 0) {
        return handle_ping_command(args, arg_count);
    }

    if (strcmp(args[0], "spy") == 0) {
        return handle_spy_command(args, arg_count);
    }

    if (strcmp(args[0], "snoop") == 0) {
        return handle_snoop_command(args, arg_count);
    }

    // Construct full string command representation for job logging
    char cmd_str[256] = "";
    for (int i = 0; i < arg_count; i++) {
        strncat(cmd_str, args[i], sizeof(cmd_str) - strlen(cmd_str) - 1);
        if (i < arg_count - 1) strncat(cmd_str, " ", sizeof(cmd_str) - strlen(cmd_str) - 1);
    }

    // 1. Construct temporary Token list from raw string args
    Token *head = NULL;
    Token *tail = NULL;
    for (int i = 0; i < arg_count; i++) {
        Token *new_tok = (Token *)malloc(sizeof(Token));
        if (!new_tok) continue;

        // Using TOKEN_WORD universally; redirection processing evaluates token string values directly
        new_tok->type = TOKEN_WORD;
        new_tok->value = strdup(args[i]);
        new_tok->next = NULL;

        if (head == NULL) {
            head = new_tok;
            tail = new_tok;
        } else {
            tail->next = new_tok;
            tail = new_tok;
        }
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("cshell: fork error");
        return 0;
    } else if (pid == 0) {
        //Create new process group for this command in the child process
        pid_t child_pid = getpid();
        setpgid(child_pid, child_pid);

        if (!is_background) {
            give_terminal_to_pgid(child_pid);
        } else {
            int dev_null = open("/dev/null", O_RDONLY);
            if (dev_null >= 0) {
                dup2(dev_null, STDIN_FILENO);
                close(dev_null);
            }
        }

        // Restore default signal handling so executable processes keyboard interrupts
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        // Child Process: Process input redirection and run binary
        char *clean_args[256];
        int clean_arg_count = 0;

        if (process_input_redirection(head, clean_args, &clean_arg_count) != 0) {
            free_tokens(head);
            exit(EXIT_FAILURE);
        }

        if (clean_arg_count == 0 || clean_args[0] == NULL) {
            free_tokens(head);
            exit(EXIT_SUCCESS);
        }

        char resolved_path[PATH_MAX];
        if (!resolve_command_path(clean_args[0], resolved_path)) {
            printf("cshell: command not found (%s)\n", clean_args[0]);
            free_tokens(head);
            exit(127);
        }

        execv(resolved_path, clean_args);
        perror("execv");
        free_tokens(head);
        exit(EXIT_FAILURE);
    } else {
        //Set process group in the parent to avoid race conditions
        setpgid(pid, pid);
        
        free_tokens(head);
        if (is_background) {
            add_process_to_job(pid, pid, cmd_str);
            Job *j = find_job_by_id(pid);
            int job_id = j ? j->job_id : pid;
            printf("[%d] %d\n", job_id, pid);
            return 1;
        } else {
            // Hand terminal control to the child process group
            give_terminal_to_pgid(pid);

            int status;
            // WUNTRACED enables waitpid to return if child process is suspended via Ctrl-Z
            waitpid(pid, &status, WUNTRACED);

            if (WIFSTOPPED(status)) {
                add_process_to_job(pid, pid, cmd_str);
                update_job_status(pid, PROCESS_STOPPED);
                Job *j = find_job_by_id(pid);
                int job_id = j ? j->job_id : pid;
                printf("\n[%d] + Stopped    %s\n", job_id, cmd_str);
            } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
                remove_job(pid);
            }

            // Reclaim foreground terminal control back to shell
            reclaim_terminal();

            if (WIFEXITED(status) && WEXITSTATUS(status) == 127) {
                return 0;
            }
        }
    }
    return 1;
}

/*This function executes an external command by resolving its path and spawning a child process. 
It first checks if any arguments were provided, resolves the binary location using resolve_command_path, 
and prints an error message if the binary was not found; otherwise, it constructs a NULL-terminated 
argument array, forks a child process to run execv with the resolved binary path, and forces the 
parent shell process to block and wait until the child process finishes execution.*/
int execute_external_command(char **args, int arg_count) {
    return execute_external_command_bg(args, arg_count, 0);
}

/* Master Command Dispatcher: Handles Built-ins (like resume) before attempting external execution */
int execute_command(char **args, int arg_count, int is_background) {
    if (arg_count == 0 || args[0] == NULL) {
        return 1;
    }

    // Intercept built-in 'resume' command
    if (strcmp(args[0], "resume") == 0) {
        return handle_resume_command(args, arg_count);
    }

    if (strcmp(args[0], "activities") == 0) {
        print_activities();
        return 1;
    }

    // Pass down to external binary handler
    return execute_external_command_bg(args, arg_count, is_background);
}