#ifndef LOCATE_H
#define LOCATE_H

#include "prompt.h"

/*This function serves as the main execution handler for the locate command. It receives the array of 
command-line arguments provided by the user along with the total count of those arguments. Its main role 
is to validate the user input to ensure at least one target command name was passed, and then loop 
through each command argument sequentially to search for matching executable binaries across both the 
current working directory and every folder listed in the system PATH environment variable.*/
void execute_locate(char **args, int arg_count);

#endif