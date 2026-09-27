#include "terminal_control.h"
#include "activities.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <termios.h>

/* Reference the head pointer defined in activities.c */
extern Job *head_job;

/* Stores the main shell process group ID so terminal control can be restored to it. */
static pid_t shell_pgid;

/*This function initializes the shell process context and configures signal handling.
 It sets SIGINT (Ctrl-C), SIGTSTP (Ctrl-Z), and SIGTTOU (terminal output control) 
 to be ignored by the main shell process so that typing keys like Ctrl-C or giving 
 terminal control away does not accidentally interrupt or suspend the shell. It also 
 ensures the shell runs inside its own distinct process group and takes control of the terminal.*/

void init_shell_signals(void) {
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);

    shell_pgid = getpid();
    setpgid(shell_pgid, shell_pgid);
    tcsetpgrp(STDIN_FILENO, shell_pgid);
}

/*This function transfers operational foreground control of the terminal to a specific 
 process group ID using tcsetpgrp. This ensures keyboard signals like Ctrl-C and Ctrl-Z 
 are sent directly by the operating system to the target command group rather than the shell.*/
void give_terminal_to_pgid(pid_t pgid) {
    signal(SIGTTOU, SIG_IGN);
    tcsetpgrp(STDIN_FILENO, pgid);
    signal(SIGTTOU, SIG_DFL);
}

/*This function reclaims terminal foreground control back to the main shell process group.
 It is called immediately after a foreground child process finishes executing or gets suspended,
 allowing the shell to draw its command prompt and receive keyboard input again.*/
void reclaim_terminal(void) {
    signal(SIGTTOU, SIG_IGN);
    tcsetpgrp(STDIN_FILENO, shell_pgid);
    signal(SIGTTOU, SIG_DFL);
}

/*This function registers a new process under a job group via add_process_to_job, sets its initial state, 
 and returns the assigned job ID.*/
int add_job(pid_t pgid, const char *cmd, ProcessState state) {
    add_process_to_job(pgid, pgid, cmd);
    update_job_status(pgid, state);

    Job *curr = head_job;
    while (curr) {
        if (curr->pgid == pgid) {
            return curr->job_id;
        }
        curr = curr->next;
    }
    return 1;
}

/*This function scans through the active job list to locate a process group matching 
 the provided process group ID and updates all processes in that group to the target ProcessState.*/
void update_job_status(pid_t pgid, ProcessState state) {
    Job *curr = head_job;
    while (curr) {
        if (curr->pgid == pgid) {
            Process *p = curr->processes;
            while (p) {
                p->state = state;
                p = p->next;
            }
            return;
        }
        curr = curr->next;
    }
}

/*This function removes a completed job from the linked list matching the given process 
 group ID and releases its allocated heap memory.*/
void remove_job(pid_t pgid) {
    Job **curr = &head_job;
    while (*curr) {
        if ((*curr)->pgid == pgid) {
            Job *temp = *curr;
            *curr = (*curr)->next;

            Process *p = temp->processes;
            while (p) {
                Process *next_p = p->next;
                free(p);
                p = next_p;
            }
            free(temp);
            return;
        }
        curr = &((*curr)->next);
    }
}

/*
 This function iterates through all tracked jobs in the linked list and checks if 
 any of them currently have a status set to JOB_STOPPED. It returns 1 if at least one 
 stopped job exists, or 0 if none are stopped.
 */
int has_stopped_jobs(void) {
    Job *curr = head_job;
    while (curr) {
        Process *p = curr->processes;
        while (p) {
            if (p->state == PROCESS_STOPPED) {
                return 1;
            }
            p = p->next;
        }
        curr = curr->next;
    }
    return 0;
}

/*
 This function iterates through all active or stopped jobs in the shell's job tracking list 
 and sends a SIGHUP (hangup) signal to each entire process group using negative PIDs. 
 This guarantees that orphaned background or stopped child processes receive termination 
 notifications when the parent shell exits.
 */
void send_sighup_to_all_jobs(void) {
    Job *curr = head_job;
    while (curr) {
        kill(-curr->pgid, SIGHUP);
        curr = curr->next;
    }
}