#ifndef PEEK_H
#define PEEK_H

#include "prompt.h"

/* Main entry point for the 'peek' command.
   Parses flags (-n, -r) and processes inputs (files or stdin). */
void execute_peek(char **args, int arg_count);

#endif