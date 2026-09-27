#ifndef HOP_H
#define HOP_H

#include <limits.h>

/* Global variable storing the previous working directory for 'hop -' and 'reveal -' */
extern char prev_working_dir[PATH_MAX];

/* Function to set up tracking variables when the shell boots up. */
void init_hop(void);

/* Main entry point for executing the 'hop' command. */
void execute_hop(char **args, int arg_count);

#endif