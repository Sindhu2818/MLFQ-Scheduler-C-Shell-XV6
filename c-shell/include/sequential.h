#ifndef SEQUENTIAL_H
#define SEQUENTIAL_H

#include "lexer.h"

/* Processes and executes multiple commands separated by semicolon (;) sequentially, 
   ensuring execution stops if any command fails to be found or launched. */
void execute_sequential_commands(Token *tokens);

#endif