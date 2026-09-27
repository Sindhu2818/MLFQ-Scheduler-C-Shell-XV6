#ifndef TERMINAL_CONTROL_H
#define TERMINAL_CONTROL_H

#include <sys/types.h>
#include "activities.h"

/* Function declarations for process group, signal, and terminal control. */
void init_shell_signals(void);
void give_terminal_to_pgid(pid_t pgid);
void reclaim_terminal(void);

/* Helper functions for job management and terminal control */
int add_job(pid_t pgid, const char *cmd, ProcessState state);
void update_job_status(pid_t pgid, ProcessState state);
void remove_job(pid_t pgid);
int has_stopped_jobs(void);
void send_sighup_to_all_jobs(void);

#endif