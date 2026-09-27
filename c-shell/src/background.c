#include "background.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_JOBS 1024

static BackgroundJob jobs[MAX_JOBS];
static int job_count = 0;
static int next_job_id = 1;

/* Signal handler: Non-blocking reap of background processes */
static void sigchld_handler(int sig) {
    (void)sig;
    int status;
    pid_t pid;

    // Use WNOHANG so the shell never blocks
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        for (int i = 0; i < job_count; i++) {
            if (jobs[i].pid == pid && !jobs[i].is_completed) {
                jobs[i].is_completed = 1;
                jobs[i].terminated_by_signal = WIFSIGNALED(status);
                break;
            }
        }
    }
}

/* Installs SIGCHLD signal handler */
void init_background_handler(void) {
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);
}

/* Registers a new background job and prints [job_id] pid */
int add_background_job(pid_t pid, const char *cmd_name) {
    int job_id = next_job_id++;

    // Print banner immediately before command output
    printf("[%d] %d\n", job_id, pid);
    fflush(stdout);

    if (job_count < MAX_JOBS) {
        jobs[job_count].job_id = job_id;
        jobs[job_count].pid = pid;
        strncpy(jobs[job_count].command_name, cmd_name ? cmd_name : "process", 255);
        jobs[job_count].command_name[255] = '\0';
        jobs[job_count].is_completed = 0;
        jobs[job_count].terminated_by_signal = 0;
        job_count++;
    }

    return job_id;
}

/* Prints notifications for any reaped jobs */
void print_completed_jobs(void) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].is_completed && jobs[i].pid > 0) {
            if (jobs[i].terminated_by_signal) {
                printf("%s with pid %d exited abnormally\n", jobs[i].command_name, jobs[i].pid);
            } else {
                printf("%s with pid %d exited normally\n", jobs[i].command_name, jobs[i].pid);
            }
            fflush(stdout);
            jobs[i].pid = -1; // Mark reported
        }
    }
}