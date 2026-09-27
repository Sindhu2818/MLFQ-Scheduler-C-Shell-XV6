#ifndef PROMPT_H
#define PROMPT_H

// Initializes the initial home directory ($H) at shell startup
void init_prompt(void);

// Formats and displays <username@hostname:currentpath>
void display_prompt(void);

/* Prototype declaration for get_home_dir */
const char *get_home_dir(void);

#endif