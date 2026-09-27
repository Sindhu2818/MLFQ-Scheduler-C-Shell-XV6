#include "pipeline.h"
#include "execute.h"
#include "redirection.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <limits.h>

/* Helper structure representing a single command stage within a pipeline */
typedef struct {
    Token *tokens; // Linked list of tokens belonging exclusively to this stage
} PipeStage;

/* Free memory allocated for temporary pipeline stage token lists */
static void free_stage_tokens(Token *head) {
    while (head) {
        Token *temp = head;
        head = head->next;
        if (temp->value) {
            free(temp->value);
        }
        free(temp);
    }
}

/* Helper function: Duplicate a token node for isolated stage processing */
static Token *duplicate_token(const Token *src) {
    if (!src) return NULL;
    Token *dest = (Token *)malloc(sizeof(Token));
    if (!dest) return NULL;
    dest->type = src->type;
    dest->value = strdup(src->value);
    dest->next = NULL;
    return dest;
}

/* Executes built-in commands directly inside child processes during pipeline execution */
static void execute_stage_builtin(Token *stage_tokens) {
    char *args[256];
    int arg_count = 0;
    
    // Process input/output redirections (<, >, >>) for this specific stage
    if (process_input_redirection(stage_tokens, args, &arg_count) != 0) {
        exit(EXIT_FAILURE);
    }

    if (arg_count == 0 || args[0] == NULL) {
        exit(EXIT_SUCCESS);
    }

    // Attempt external execution if not handled by custom built-ins
    execute_external_command(args, arg_count);
    exit(EXIT_SUCCESS);
}

/* Central pipeline handler: Creates inter-process pipes, forks child processes, 
   connects file descriptors, handles missing commands gracefully, and waits for completion */
void execute_pipeline(Token *tokens) {
    if (!tokens) return;

    // 1. Split full token list into distinct pipeline stages delimited by '|'
    PipeStage stages[64];
    int stage_count = 0;
    
    Token *curr = tokens;
    Token *stage_head = NULL;
    Token *stage_tail = NULL;

    while (curr != NULL && curr->type != TOKEN_EOF) {
        if (curr->type == TOKEN_PIPE) {
            stages[stage_count++].tokens = stage_head;
            stage_head = NULL;
            stage_tail = NULL;
        } else {
            Token *tok_copy = duplicate_token(curr);
            if (stage_head == NULL) {
                stage_head = tok_copy;
                stage_tail = tok_copy;
            } else {
                stage_tail->next = tok_copy;
                stage_tail = tok_copy;
            }
        }
        curr = curr->next;
    }
    // Add final stage after the last pipe operator
    if (stage_head != NULL) {
        stages[stage_count++].tokens = stage_head;
    }

    if (stage_count == 0) return;

    // If no pipe operator was found, return immediately to default main handler
    if (stage_count == 1) {
        free_stage_tokens(stages[0].tokens);
        return;
    }

    // 2. Allocate pipe file descriptors (N-1 pipes needed for N stages)
    int pipefds[2 * (stage_count - 1)];
    for (int i = 0; i < stage_count - 1; i++) {
        if (pipe(pipefds + i * 2) < 0) {
            perror("pipe");
            for (int k = 0; k < stage_count; k++) {
                free_stage_tokens(stages[k].tokens);
            }
            return;
        }
    }

    pid_t pids[64];

    // 3. Fork a child process for each stage in the pipeline
    for (int i = 0; i < stage_count; i++) {
        pids[i] = fork();

        if (pids[i] < 0) {
            perror("fork");
            break;
        }

        if (pids[i] == 0) {
            // --- Child Process Setup ---

            // Redirect STDIN from previous stage pipe read-end (if not the first stage)
            if (i > 0) {
                if (dup2(pipefds[(i - 1) * 2], STDIN_FILENO) < 0) {
                    perror("dup2 read");
                    exit(EXIT_FAILURE);
                }
            }

            // Redirect STDOUT to next stage pipe write-end (if not the last stage)
            if (i < stage_count - 1) {
                if (dup2(pipefds[i * 2 + 1], STDOUT_FILENO) < 0) {
                    perror("dup2 write");
                    exit(EXIT_FAILURE);
                }
            }

            // Close all inherited pipe file descriptors inside child process before executing
            for (int k = 0; k < 2 * (stage_count - 1); k++) {
                close(pipefds[k]);
            }

            // Execute stage logic with redirection support (<, >, >>)
            execute_stage_builtin(stages[i].tokens);
            exit(EXIT_SUCCESS);
        }
    }

    // 4. Parent Process: Close all pipe file descriptors immediately after forking all children
    for (int i = 0; i < 2 * (stage_count - 1); i++) {
        close(pipefds[i]);
    }

    // 5. Parent Process: Wait for all child stage processes to finish
    for (int i = 0; i < stage_count; i++) {
        int status;
        waitpid(pids[i], &status, 0);
        free_stage_tokens(stages[i].tokens);
    }
}