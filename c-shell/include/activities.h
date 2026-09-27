#ifndef ACTIVITIES_H
#define ACTIVITIES_H

#include <sys/types.h>

/* The ProcessState enum defines the current lifecycle state of an individual process 
   within a job group. It tracks whether a process is currently running or paused. */
typedef enum {
    PROCESS_RUNNING,
    PROCESS_STOPPED
} ProcessState;

/* The Process structure represents a single command running inside a job pipeline. 
   It forms a linked list node holding the OS process ID (pid), the exact command string 
   launched, its current state, and a pointer to the next process in the pipeline. */
typedef struct Process {
    pid_t pid;
    char command[128];
    ProcessState state;
    struct Process *next;
} Process;

/* The Job structure represents an entire process group created by the shell. 
   It tracks a unique incremental job index, the process group ID (pgid), a linked list 
   of all processes belonging to this pipeline, and a pointer to the next active job. */
typedef struct Job {
    int job_id;
    pid_t pgid;
    Process *processes;
    struct Job *next;
} Job;

/* This function registers a new process under a specific job process group. If the job group 
   does not exist yet, it allocates a new job structure with an incremented job ID and appends 
   the process to its pipeline execution list. */
void add_process_to_job(pid_t pgid, pid_t pid, const char *cmd);

/* This function updates the status of all tracked process groups and outputs the active ones. 
   It sweeps through every background process, reaps any processes that have terminated, updates 
   the status of stopped or resumed processes, and prints the formatted job tree. */
void print_activities(void);
Job *find_job_by_id(int job_id);
Job *find_job_by_pid(pid_t pid);
void update_job_states(void);
void update_job_status(pid_t pgid, ProcessState state);
void remove_job(pid_t pgid);
#endif
