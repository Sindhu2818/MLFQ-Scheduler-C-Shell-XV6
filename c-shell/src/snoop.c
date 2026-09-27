#include "snoop.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <sys/reg.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>

#define MAX_SYSCALLS 1024

// Structure used to store tracking details for each unique system call made
typedef struct {
    int syscall_num;
    long long count;
    double total_time;
    int order;
} SyscallStat;

static SyscallStat stats[MAX_SYSCALLS];
static int total_unique_syscalls = 0;

/*This function converts a numeric system call number into its corresponding
human-readable name string (e.g., system call number 0 becomes "read").
It returns NULL if the system call number isn't present in the lookup table.*/
static const char *get_syscall_name(int num) {
    switch (num) {
        /* Basic file and memory related system calls */
        case 0:   return "read";
        case 1:   return "write";
        case 2:   return "open";
        case 3:   return "close";
        case 4:   return "stat";
        case 5:   return "fstat";
        case 6:   return "lstat";
        case 7:   return "poll";
        case 8:   return "lseek";
        case 9:   return "mmap";
        case 10:  return "mprotect";
        case 11:  return "munmap";
        case 12:  return "brk";

        /* Signal related system calls */
        case 13:  return "rt_sigaction";
        case 14:  return "rt_sigprocmask";

        case 16:  return "ioctl";
        case 17:  return "pread64";

        case 21:  return "access";

        /* Sleep / time related system calls */
        case 35:  return "nanosleep";

        /* Process / execution related system calls */
        case 39:  return "getpid";
        case 59:  return "execve";
        case 60:  return "exit";

        /* File system related system calls */
        case 62:  return "lseek";
        case 72:  return "fcntl";
        case 79:  return "fstat";

        /* Time related system calls */
        case 96:  return "gettimeofday";
        case 97:  return "getrlimit";

        /* Process / signal related calls */
        case 131: return "tgkill";
        case 137: return "statfs";
        case 157: return "prctl";
        case 158: return "arch_prctl";
        case 186: return "gettid";

        /* Scheduling / synchronization */
        case 202: return "futex";
        case 204: return "sched_getaffinity";

        /* Directory / process information */
        case 217: return "getdents64";

        /* Process ID / thread related */
        case 218: return "set_tid_address";
        case 219: return "restart_syscall";

        /* Memory / sleep related */
        case 230: return "clock_nanosleep";
        case 231: return "exit_group";

        /* Modern Linux system calls */
        case 257: return "openat";
        case 273: return "set_robust_list";
        case 302: return "prlimit64";
        case 318: return "getrandom";
        case 332: return "statx";
        case 334: return "rseq";
        default: return NULL;
    }
}

/*This function adds timing data for a system call execution.
If the system call has already been recorded, it increases its call count
and adds the duration to its accumulated total time. If it is the first time 
seeing this system call, it creates a new entry and saves its order of occurrence.*/
static void record_syscall(int num, double duration) {
    for (int i = 0; i < total_unique_syscalls; i++) {
        if (stats[i].syscall_num == num) {
            stats[i].count++;
            stats[i].total_time += duration;
            return;
        }
    }
    if (total_unique_syscalls < MAX_SYSCALLS) {
        stats[total_unique_syscalls].syscall_num = num;
        stats[total_unique_syscalls].count = 1;
        stats[total_unique_syscalls].total_time = duration;
        stats[total_unique_syscalls].order = total_unique_syscalls;
        total_unique_syscalls++;
    }
}

/*
This comparison function is used by qsort to order the system calls.
It sorts the results primarily by call count in descending order (highest count first).
If two system calls have the same count, it breaks the tie by sorting them 
based on which system call occurred first in time. */
static int compare_stats(const void *a, const void *b) {
    const SyscallStat *s1 = (const SyscallStat *)a;
    const SyscallStat *s2 = (const SyscallStat *)b;

    if (s1->count != s2->count) {
        return (s2->count > s1->count) ? -1 : 1; // Sort descending by call count
    }
    return s1->order - s2->order;
}

/*This function sorts the accumulated statistics array and prints out the final 
summary table showing the system call name, execution count, and total elapsed 
time formatted in seconds.*/
static void print_summary(void) {
    if (total_unique_syscalls == 0) return;
    qsort(stats, total_unique_syscalls, sizeof(SyscallStat), compare_stats);

    printf("%-15s %-7s %s\n", "syscall", "calls", "time");
    for (int i = 0; i < total_unique_syscalls; i++) {
        const char *name = get_syscall_name(stats[i].syscall_num);
        char name_buf[32];
        if (!name) {
            snprintf(name_buf, sizeof(name_buf), "syscall_%d", stats[i].syscall_num);
            name = name_buf;
        }
        printf("%-15s %-7lld %.3fs\n", name, stats[i].count, stats[i].total_time);
    }
    fflush(stdout);
}

/*This is the main entry handler for the "snoop" shell command.
It validates arguments, connects to a target process (either by spawning and 
executing a new process or by attaching to an existing PID using ptrace), intercepts 
each system call entry and exit to track counts and measure execution time, 
and prints the final summary upon process termination.*/
int handle_snoop_command(char **args, int arg_count) {
    if (arg_count < 2) {
        printf("snoop: invalid syntax\n");
        fflush(stdout);
        return 1;
    }

    total_unique_syscalls = 0;
    pid_t target_pid = -1;
    int is_attach = 0;

    if (strcmp(args[1], "-p") == 0) {
        if (arg_count != 3) {
            printf("snoop: invalid syntax\n");
            fflush(stdout);
            return 1;
        }
        for (int i = 0; args[2][i] != '\0'; i++) {
            if (!isdigit((unsigned char)args[2][i])) {
                printf("snoop: invalid syntax\n");
                fflush(stdout);
                return 1;
            }
        }

        target_pid = (pid_t)atoi(args[2]);
        is_attach = 1;
    }

    int status;

    if (is_attach) {
        if (ptrace(PTRACE_ATTACH, target_pid, NULL, NULL) < 0) {
            if (errno == ESRCH) {
                printf("snoop: no such process\n");
            } else if (errno == EPERM) {
                printf("snoop: operation not permitted\n");
            } else {
                printf("snoop: ptrace attach failed\n");
            }
            fflush(stdout);
            return 1;
        }
        
        // Wait specifically for the target process to receive the ATTACH SIGSTOP
        if (waitpid(target_pid, &status, 0) < 0) {
            perror("waitpid");
            return 1;
        }

        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            printf("snoop: process terminated before tracing\n");
            fflush(stdout);
            return 1;
        }
    } else {
        target_pid = fork();
        if (target_pid == 0) {
            ptrace(PTRACE_TRACEME, 0, NULL, NULL);
            raise(SIGSTOP);
            execvp(args[1], &args[1]);
            // If execvp returns, command was not found
            fprintf(stderr, "snoop: command not found\n");
            fflush(stderr);
            _exit(127);
        } else if (target_pid < 0) {
            perror("fork");
            return 1;
        }

        // Wait for child's initial raise(SIGSTOP)
        if (waitpid(target_pid, &status, 0) < 0) {
            perror("waitpid");
            return 1;
        }
    }

    // Set options only after process is confirmed stopped by waitpid
    if (ptrace(PTRACE_SETOPTIONS, target_pid, 0, PTRACE_O_TRACESYSGOOD) < 0) {
        printf("snoop: ptrace setup failed\n");
        fflush(stdout);

        if (is_attach) {
            ptrace(PTRACE_DETACH, target_pid, NULL, NULL);
        }

        return 1;
    }

    // Main Tracer Loop
    int in_syscall = 0;
    struct timespec start_time;
    int current_syscall = -1;
    int exec_failed = 0;

    while (1) {
        if (ptrace(PTRACE_SYSCALL, target_pid, NULL, NULL) < 0) break;

        if (waitpid(target_pid, &status, 0) < 0) {
            if(errno==EINTR){
                continue;
            }
            break;
        }

        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) == 127) {
                exec_failed = 1;
            }
            break;
        }
        if (WIFSIGNALED(status)) break;

        if (WIFSTOPPED(status)) {
            int sig = WSTOPSIG(status);

            // Handle system call stop (either TRACESYSGOOD or standard SIGTRAP)
            if (sig == (SIGTRAP | 0x80)) {
                struct user_regs_struct regs;
                if (ptrace(PTRACE_GETREGS, target_pid, NULL, &regs) < 0) break;

                if (!in_syscall) {
                    // Syscall Entry
                    in_syscall = 1;
                    current_syscall = (int)regs.orig_rax;
                    clock_gettime(CLOCK_MONOTONIC, &start_time);
                } else {
                    // Syscall Exit
                    in_syscall = 0;
                    struct timespec end_time;
                    clock_gettime(CLOCK_MONOTONIC, &end_time);

                    double duration = (end_time.tv_sec - start_time.tv_sec) +
                                      (end_time.tv_nsec - start_time.tv_nsec) / 1e9;

                    record_syscall(current_syscall, duration);
                }
            }
        }
    }

    if (!exec_failed) {
        print_summary();
    }

    if (is_attach) {
        ptrace(PTRACE_DETACH, target_pid, NULL, NULL);
    }

    return 1;
}