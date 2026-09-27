#ifndef PIPELINE_H
#define PIPELINE_H

#include "lexer.h"

/* Executes a command pipeline (e.g., cmd1 | cmd2 | cmd3) with full support 
   for file redirection (<, >, >>) on individual pipeline stages. */
void execute_pipeline(Token *tokens);

#endif 