#ifndef RESUME_H
#define RESUME_H

#include <sys/types.h>

/*Parses user input tokens for the 'resume' built-in command, validates syntax,
retrieves the target job by its job ID, and resumes process execution in either
the background (bg) or foreground (fg), handling optional execution timeouts.
@param args Array of string tokens representing command line arguments
@param arg_count Total number of argument tokens passed
@return 1 on completion or standard error handling, 0 on critical system failure*/
int handle_resume_command(char **args, int arg_count);

#endif