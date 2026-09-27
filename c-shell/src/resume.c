#include "resume.h"
#include "activities.h"
#include "terminal_control.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>

/* Global flag used to indicate when SIGALRM timeout has fired */
static volatile sig_atomic_t g_timeout_triggered = 0;

/*Signal handler executed when SIGALRM is triggered by alarm().
Sets a flag indicating that the specified foreground timeout has elapsed.*/
static void sigalrm_handler(int sig) {
    (void)sig;
    g_timeout_triggered = 1;
}

/* Handles validation, job lookup, process signaling, and foreground/background state updates 
for the 'resume' command.*/
int handle_resume_command(char **args, int arg_count) {
    // Validate basic syntax structure and mandatory job number prefix (%)
    if (arg_count < 3 || args[1][0] != '%') {
        printf("resume: invalid syntax\n");
        return 1;
    }

    // Extract job ID numerical value from string token
    int job_id = atoi(&args[1][1]);
    if (job_id <= 0) {
        printf("resume: invalid syntax\n");
        return 1;
    }

    // Parse execution mode (fg vs bg) and mandatory/optional argument options
    int is_fg = 0;
    int timeout_sec = -1;

    if (strcmp(args[2], "bg") == 0) {
        if (arg_count != 3) {
            printf("resume: invalid syntax\n");
            return 1;
        }
        is_fg = 0;
    }else if (strcmp(args[2], "fg") == 0){
        is_fg = 1;
        if (arg_count == 5) {
            if (strcmp(args[3], "--timeout") != 0) {
                printf("resume: invalid syntax\n");
                return 1;
            }
            timeout_sec = atoi(args[4]);
            if (timeout_sec <= 0) {
                printf("resume: invalid syntax\n");
                return 1;
            }
        } else if (arg_count != 3) {
            printf("resume: invalid syntax\n");
            return 1;
        }
    }else {
        printf("resume: invalid syntax\n");
        return 1;
    }

    // Locate the active job inside the shell's job tracking table
    Job *job = find_job_by_id(job_id);
    if (job == NULL) {
        printf("resume: no such job\n");
        return 1;
    }
    pid_t pgid = job->pgid;
    const char *cmd_name = job->processes->command;

    // Background execution pathway: print status, set state, send SIGCONT, yield control
    if (!is_fg) {
        printf("[%d] + Running    %s\n", job->job_id, cmd_name);
        update_job_status(pgid, PROCESS_RUNNING);
        
        if (kill(-pgid, SIGCONT) < 0) {
            perror("resume: kill (SIGCONT)");
        }
        return 1;
    }

    // Foreground execution pathway: print command, transfer terminal, set state, send SIGCONT
    printf("%s\n", cmd_name);
    update_job_status(pgid, PROCESS_RUNNING);

    give_terminal_to_pgid(pgid);

    if (kill(-pgid, SIGCONT) < 0) {
        perror("resume: kill (SIGCONT)");
        reclaim_terminal();
        return 1;
    }

    // Configure timeout signal handling if --timeout argument was passed
    struct sigaction sa_old, sa_new;
    if (timeout_sec > 0) {
        g_timeout_triggered = 0;
        memset(&sa_new, 0, sizeof(sa_new));
        sa_new.sa_handler = sigalrm_handler;
        sigemptyset(&sa_new.sa_mask);
        sa_new.sa_flags = 0; // Ensures waitpid() unblocks on SIGALRM with EINTR
        sigaction(SIGALRM, &sa_new, &sa_old);
        alarm(timeout_sec);
    }

    // Block shell execution while child process group runs
    int status;
    pid_t res = waitpid(-pgid, &status, WUNTRACED);

    // Cancel countdown timer if process finishes before timeout
    if (timeout_sec > 0) {
        alarm(0);
        sigaction(SIGALRM, &sa_old, NULL); // Restore original handler
    }

    // Handle process completion, suspension, or timeout termination state
    if (timeout_sec > 0 && res == -1 && errno == EINTR && g_timeout_triggered) {
        printf("resume: job timed out\n");
        
        kill(-pgid, SIGTERM);
        waitpid(-pgid, &status, 0); // Harvest process group zombie state
        
        remove_job(pgid);
    } else {
        if (WIFSTOPPED(status)) {
            update_job_status(pgid, PROCESS_STOPPED);
        } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
            remove_job(pgid);
        }
    }

    // Reclaim control of terminal stdin back to shell process group
    reclaim_terminal();

    return 1;
}
    