#include "activities.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

Job *head_job = NULL;
static int next_job_id = 1;

/* This function adds a process into the internal job tracking list. It searches for an 
   existing job matching the given process group ID (pgid). If it finds one, it appends the 
   new process to that job's process list. If no matching job group exists, it creates a new 
   job node, assigns it the next available job ID, and links it into the global job list. */

void add_process_to_job(pid_t pgid, pid_t pid, const char *cmd) {
    Job *curr_job = head_job;
    Job *prev_job = NULL;

    while (curr_job != NULL && curr_job->pgid != pgid) {
        prev_job = curr_job;
        curr_job = curr_job->next;
    }

    if (curr_job == NULL) {
        curr_job = (Job *)malloc(sizeof(Job));
        if (!curr_job) return;

        curr_job->job_id = next_job_id++;
        curr_job->pgid = pgid;
        curr_job->processes = NULL;
        curr_job->next = NULL;

        if (prev_job == NULL) {
            head_job = curr_job;
        } else {
            prev_job->next = curr_job;
        }
    }

    Process *new_proc = (Process *)malloc(sizeof(Process));
    if (!new_proc) return;

    new_proc->pid = pid;
    strncpy(new_proc->command, cmd, sizeof(new_proc->command) - 1);
    new_proc->command[sizeof(new_proc->command) - 1] = '\0';
    new_proc->state = PROCESS_RUNNING;
    new_proc->next = NULL;

    if (curr_job->processes == NULL) {
        curr_job->processes = new_proc;
    } else {
        Process *p = curr_job->processes;
        while (p->next != NULL) {
            p = p->next;
        }
        p->next = new_proc;
    }
}

/* This function queries the operating system kernel for status changes across all tracked 
   child processes using a non-blocking waitpid call. It updates process states to Running 
   or Stopped when signals change their state, and unlinks processes that have finished 
   or crashed. If a job group becomes completely empty because all its child processes have 
   exited, it cleans up and removes the job entry entirely. */

void update_job_states(void) {
    Job *curr_job = head_job;
    Job *prev_job = NULL;

    while (curr_job != NULL) {
        Process *curr_proc = curr_job->processes;
        Process *prev_proc = NULL;

        while (curr_proc != NULL) {
            int status;
            pid_t res = waitpid(curr_proc->pid, &status, WNOHANG | WUNTRACED | WCONTINUED);

            if (res > 0) {
                if (WIFSTOPPED(status)) {
                    curr_proc->state = PROCESS_STOPPED;
                    prev_proc = curr_proc;
                    curr_proc = curr_proc->next;
                    } else if (WIFCONTINUED(status)) {
                    curr_proc->state = PROCESS_RUNNING;
                    prev_proc = curr_proc;
                    curr_proc = curr_proc->next;
                } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
                    Process *to_free = curr_proc;
                    if (prev_proc == NULL) {
                        curr_job->processes = curr_proc->next;
                        curr_proc = curr_job->processes;
                    } else {
                        prev_proc->next = curr_proc->next;
                        curr_proc = curr_proc->next;
                    }
                    free(to_free);
                }
            }else {
                prev_proc = curr_proc;
                curr_proc = curr_proc->next;
            }
        }

        if (curr_job->processes == NULL) {
            Job *job_to_free = curr_job;
            if (prev_job == NULL) {
                head_job = curr_job->next;
                curr_job = head_job;
            } else {
                prev_job->next = curr_job->next;
                curr_job = curr_job->next;
            }
            free(job_to_free);
        } else {
            prev_job = curr_job;
            curr_job = curr_job->next;
        }
    }
}

/* This function executes the main user-facing activities command logic. It first invokes 
   update_job_states to purge exited child processes and update live job statuses. Then, 
   it iterates through all remaining process groups in chronological order, printing the 
   group banner line followed by each individual process's PID, executable name, and state 
   indented beneath it. */
void print_activities(void) {
    update_job_states();

    Job *j = head_job;
    while (j != NULL) {
        printf("[%d] pgid %d\n", j->job_id, j->pgid);

        Process *p = j->processes;
        while (p != NULL) {
            const char *state_str = (p->state == PROCESS_STOPPED) ? "Stopped" : "Running";
            printf("  %d %s %s\n", p->pid, p->command, state_str);
            p = p->next;
        }

        j = j->next;
    }
}

/* Searches the global job list head_job for a matching numerical job_id */
Job *find_job_by_id(int job_id) {
    Job *curr = head_job;
    while (curr != NULL) {
        if (curr->job_id == job_id) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

/* Searches the global job list head_job for a process matching a specific PID */
Job *find_job_by_pid(pid_t pid) {
    update_job_states();
    Job *curr_job = head_job;
    while (curr_job != NULL) {
        Process *p = curr_job->processes;
        while (p != NULL) {
            if (p->pid == pid) {
                return curr_job;
            }
            p = p->next;
        }
        curr_job = curr_job->next;
    }
    return NULL;
}