#include <sys/types.h>
#include <sys/wait.h>
#include "ping.h"
#include "activities.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>

/* Helper function to check if a string consists strictly of digits */
static int is_non_negative_number(const char *str) {
    if (!str || *str == '\0') return 0;
    for (int i = 0; str[i] != '\0'; i++) {
        if (!isdigit((unsigned char)str[i])) {
            return 0;
        }
    }
    return 1;
}

/* Helper function to find a job by its sequential job ID (%1, %2, etc.) */
static Job *get_job_by_job_id(int job_id) {
    // Try standard find_job_by_id first
    Job *j = find_job_by_id(job_id);
    if (j) return j;

    return NULL;
}

/* Helper function to check if a PID belongs to any tracked process/job in our shell */
static Job *get_job_by_pid(pid_t pid) {
    // Search using find_job_by_id in case IDs match PIDs directly
    Job *j = find_job_by_id((int)pid);
    if (j && (j->pgid == pid)) return j;

    return NULL;
}

int handle_ping_command(char **args, int arg_count) {
    // 1. Validate argument count: ping <target> <signal_number>
    if (arg_count != 3) {
        printf("ping: invalid syntax\n");
        return 1;
    }

    const char *target_str = args[1];
    const char *sig_str = args[2];

    // 2. REQUIREMENT: Validate signal_number FIRST before resolving target
    if (!is_non_negative_number(sig_str)) {
        printf("ping: invalid syntax\n");
        return 1;
    }

    int raw_sig = atoi(sig_str);
    int actual_sig = raw_sig % 64;

    // 3. Resolve Target: Job Group (%<num>) vs PID (<num>)
    if (target_str[0] == '%') {
        if (!is_non_negative_number(&target_str[1])) {
            printf("ping: invalid syntax\n");
            return 1;
        }

        int job_id = atoi(&target_str[1]);
        Job *job = get_job_by_job_id(job_id);
        if (!job) {
            printf("ping: no such process found\n");
            return 1;
        }

        // Send signal to process group
        if (kill(-job->pgid, actual_sig) < 0) {
            printf("ping: no such process found\n");
            return 1;
        }

        printf("Sent signal %d to %s\n", raw_sig, target_str);

        // Reap process if killed
        if (actual_sig == SIGKILL || actual_sig == SIGTERM || actual_sig == SIGINT) {
            int status;
            waitpid(-job->pgid, &status, WNOHANG);
        }
    } else {
        if (!is_non_negative_number(target_str)) {
            printf("ping: invalid syntax\n");
            return 1;
        }

        pid_t target_pid = (pid_t)atoi(target_str);

        // Verify process existence in OS
        if (kill(target_pid, 0) < 0) {
            printf("ping: no such process found\n");
            return 1;
        }

        // Verify process belongs to our shell
        Job *job = get_job_by_pid(target_pid);
        if (!job && target_pid != getpid()) {
            printf("ping: no such process found\n");
            return 1;
        }

        // Send signal to specific PID
        if (kill(target_pid, actual_sig) < 0) {
            printf("ping: no such process found\n");
            return 1;
        }

        printf("Sent signal %d to %s\n", raw_sig, target_str);

        if (actual_sig == SIGKILL || actual_sig == SIGTERM || actual_sig == SIGINT) {
            int status;
            waitpid(target_pid, &status, WNOHANG);
        }
    }

    return 1;
}