#ifndef REDIRECTION_H
#define REDIRECTION_H

#include "lexer.h"
/* Handles input redirection using '<'. Checks that all input files
can be read, combines multiple input files in the given order,
and redirects standard input to the combined data using dup2()*/
int process_input_redirection(Token *tokens, char **clean_args, int *clean_arg_count);

#endif