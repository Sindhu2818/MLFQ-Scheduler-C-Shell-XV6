#include "sequential.h"
#include "execute.h"
#include "pipeline.h"
#include "background.h"
#include "hop.h"
#include "reveal.h"
#include "peek.h"
#include "locate.h"
#include "activities.h"
#include "resume.h"
#include "ping.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* This function creates an exact, independent copy of a single token structure. 
   It allocates new memory for both the token container and its string value, 
   allowing commands to be safely split and processed without corrupting the original token list. */
static Token *duplicate_token(const Token *src) {
    if (!src) return NULL;
    Token *dest = (Token *)malloc(sizeof(Token));
    if (!dest) return NULL;
    dest->type = src->type;
    dest->value = src->value ? strdup(src->value) : NULL;
    dest->next = NULL;
    return dest;
}

/* This function frees all memory associated with a dynamically allocated linked list 
   of tokens. It iterates through the list, freeing each token's string value first 
   and then freeing the token structure itself to prevent memory leaks. */
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

/* This function checks whether a token list contains a pipe operator (|). 
   It inspects each token sequentially to help determine if a command segment 
   should be delegated to the pipeline module or executed directly. */
static int contains_pipe(Token *tokens) {
    Token *curr = tokens;
    while (curr) {
        if (curr->value && strcmp(curr->value, "|") == 0) {
            return 1;
        }
        curr = curr->next;
    }
    return 0;
}

/*
 This function evaluates and executes a single command segment within a sequence or background stream.
 It checks if the segment contains pipelines, built-in functions (hop, reveal, peek, locate), or external
 commands, passing the background execution flag to the execution engine and returning execution status.
 */
static int execute_single_command_segment(Token *cmd_tokens, int is_background) {
    if (!cmd_tokens || !cmd_tokens->value) return 1;

    if (contains_pipe(cmd_tokens)) {
        execute_pipeline(cmd_tokens);
        return 1;
    }

    if (strcmp(cmd_tokens->value, "hop") == 0) {
        char *args[256];
        int arg_count = 0;
        Token *curr = cmd_tokens->next;
        while (curr && curr->value && arg_count < 255) {
            args[arg_count++] = curr->value;
            curr = curr->next;
        }
        args[arg_count] = NULL;
        execute_hop(args, arg_count);
        return 1;
    } else if (strcmp(cmd_tokens->value, "reveal") == 0) {
        char *args[256];
        int arg_count = 0;
        Token *curr = cmd_tokens->next;
        while (curr && curr->value && arg_count < 255) {
            args[arg_count++] = curr->value;
            curr = curr->next;
        }
        args[arg_count] = NULL;
        execute_reveal(args, arg_count);
        return 1;
    } else if (strcmp(cmd_tokens->value, "peek") == 0) {
        char *args[256];
        int arg_count = 0;
        Token *curr = cmd_tokens->next;
        while (curr && curr->value && arg_count < 255) {
            args[arg_count++] = curr->value;
            curr = curr->next;
        }
        args[arg_count] = NULL;
        execute_peek(args, arg_count);
        return 1;
    } else if (strcmp(cmd_tokens->value, "locate") == 0) {
        char *args[256];
        int arg_count = 0;
        Token *curr = cmd_tokens->next;
        while (curr && curr->value && arg_count < 255) {
            args[arg_count++] = curr->value;
            curr = curr->next;
        }
        args[arg_count] = NULL;
        execute_locate(args, arg_count);
        return 1;
    } else if (strcmp(cmd_tokens->value, "activities") == 0) {
        print_activities();
        return 1;
    }else if (strcmp(cmd_tokens->value, "resume") == 0) {
        char *args[256];
        int arg_count = 0;
        Token *curr = cmd_tokens;
        while (curr != NULL && curr->value != NULL && arg_count < 255) {
            args[arg_count++] = curr->value;
            curr = curr->next;
        }
        args[arg_count] = NULL;
        return handle_resume_command(args, arg_count);
    } else if (strcmp(cmd_tokens->value, "ping") == 0) {
        char *args[256];
        int arg_count = 0;
        Token *curr = cmd_tokens;
        while (curr != NULL && curr->value != NULL && arg_count < 255) {
            args[arg_count++] = curr->value;
            curr = curr->next;
        }
        args[arg_count] = NULL;
        return handle_ping_command(args, arg_count);
    }else {
        char *args[256];
        int arg_count = 0;
        Token *curr = cmd_tokens;
        while (curr != NULL && curr->value != NULL && arg_count < 255) {
            args[arg_count++] = curr->value;
            curr = curr->next;
        }
        args[arg_count] = NULL;

        if (arg_count > 0) {
            return execute_external_command_bg(args, arg_count, is_background);
        }
        return 1;
    }
}

/* This function coordinates the sequential execution of commands separated by semicolons (;) and ampersands (&). 
   It parses the full token list into distinct command sub-lists and runs them one by one in order. 
   If a command fails to start or resolve, execution halts immediately and remaining commands are skipped. */
void execute_sequential_commands(Token *tokens) {
    if (!tokens) return;

    Token *curr = tokens;
    Token *segment_head = NULL;
    Token *segment_tail = NULL;

    while (curr != NULL) {
        int is_seq_delimiter = (curr->value && strcmp(curr->value, ";") == 0);
        int is_bg_delimiter = (curr->value && strcmp(curr->value, "&") == 0);

        if (is_seq_delimiter || is_bg_delimiter) {
            if (segment_head != NULL) {
                int status = execute_single_command_segment(segment_head, is_bg_delimiter);
                free_stage_tokens(segment_head);
                segment_head = NULL;
                segment_tail = NULL;

                if (!status && is_seq_delimiter) {
                    return;
                }
            }
        } else {
            Token *tok_copy = duplicate_token(curr);
            if (tok_copy) {
                if (segment_head == NULL) {
                    segment_head = tok_copy;
                    segment_tail = tok_copy;
                } else {
                    segment_tail->next = tok_copy;
                    segment_tail = tok_copy;
                }
            }
        }
        curr = curr->next;
    }

    if (segment_head != NULL) {
        execute_single_command_segment(segment_head, 0);
        free_stage_tokens(segment_head);
    }
}
