#include "prompt.h"
#include "lexer.h"
#include "hop.h"
#include "reveal.h"
#include "peek.h"
#include "locate.h"
#include "execute.h"
#include "redirection.h"
#include "pipeline.h"
#include "sequential.h"
#include "background.h"
#include "activities.h"
#include "terminal_control.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INPUT_LEN 4096

/*This main function serves as the central control loop for the custom interactive C shell.
It initializes system prompt paths and state variables, displays the formatted prompt, reads input lines,
handles signal/EOF exits, strips invalid trailing operators, and delegates token lists to built-in handlers
(hop, reveal, peek, locate) or falls back to execute_external_command for running system executables with 
full input redirection support.*/
int main(void) {
    char input[MAX_INPUT_LEN];

    /// Initialize shell signals, process group isolation, home path, and background subsystem
    init_shell_signals();
    init_prompt();
    init_hop();
    init_background_handler();
    
    int warned_stopped_jobs = 0;

    while (1) {
        // Print notifications for any reaped background jobs before rendering prompt
        print_completed_jobs();

        // 1. Display the formatted shell prompt
        display_prompt();

        // 2. Read line from standard input
        if (fgets(input, sizeof(input), stdin) == NULL) {
            // Check for stopped background/suspended jobs on Ctrl-D (EOF)
            if (has_stopped_jobs()) {
                if (!warned_stopped_jobs) {
                    printf("\ncshell: there are stopped jobs\n");
                    warned_stopped_jobs = 1;
                    clearerr(stdin); // Clear stream EOF flag so prompt continues accepting input
                    continue; // Do not exit on first attempt
                }
            }

            //clean exit path: second conservative Ctrl-D or no stopped jobs
            printf("\n");
            send_sighup_to_all_jobs();
            exit(0);
        }

        // Reset warning state if input string was provided
        warned_stopped_jobs = 0;

        // 3. Remove trailing newline character
        size_t len = strlen(input);
        if (len > 0 && input[len - 1] == '\n') {
            input[len - 1] = '\0';
        }

        // 4. Ignore empty lines (e.g., user just hits Enter)
        if (input[0] == '\0') {
            continue;
        }

        // Lexical analysis of raw input string
        Token *tokens = NULL;
        if (lex_line(input, &tokens) != 0) {
            printf("cshell: invalid syntax\n");
            continue;
        }

        // Parse token validity
        if (!parse_line(tokens)) {
            printf("cshell: invalid syntax\n");
            free_tokens(tokens);
            continue;
        }

        // Check if the command entered is the built-in 'activities' command
        if (tokens != NULL && tokens->value != NULL && strcmp(tokens->value, "activities") == 0 && tokens->next == NULL) {
            print_activities();
            free_tokens(tokens);
            continue;
        }
        
        // 7. Pass full token stream to sequential executor (handles ;, pipes, builtins, and externals)
        execute_sequential_commands(tokens);

        // 8. Clean up token memory after execution finishes
        free_tokens(tokens);
    }

    return 0;
}