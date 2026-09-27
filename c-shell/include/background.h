#ifndef BACKGROUND_H
#define BACKGROUND_H

#include <sys/types.h>

typedef struct {
    int job_id;
    pid_t pid;
    char command_name[256];
    int is_completed;
    int terminated_by_signal;
} BackgroundJob;

void init_background_handler(void);
int add_background_job(pid_t pid, const char *cmd_name);
void print_completed_jobs(void);

#endif