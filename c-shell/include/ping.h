#ifndef PING_H
#define PING_H

/* Handles validation, target lookup, signal computation, and delivery
   for the 'ping' built-in command. */
int handle_ping_command(char **args, int arg_count);

#endif