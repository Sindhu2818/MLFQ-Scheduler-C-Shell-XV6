#ifndef EXECUTE_H
#define EXECUTE_H

#include "lexer.h"

/*This function acts as the main execution handler for external system commands with full 
support for input redirection. It accepts an array of string arguments representing the 
parsed command and its options along with the total count of those arguments, creates an 
internal token representation to process any input redirection targets, resolves the exact 
binary path following assignment precedence rules, and forks a child process to execute 
the target program using execv while the parent shell process waits for completion.*/
int execute_external_command(char **args, int arg_count);
int execute_external_command_bg(char **args, int arg_count, int is_background);

#endif