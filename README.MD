# Mini Project 1 -- C Shell and xv6 MLFQ Scheduler

This repository contains the two main parts of Mini Project 1:

1.  **C Shell (`c-shell/`)** -- a custom POSIX C shell supporting
    built-in commands, command parsing, pipelines, redirection,
    sequential/background execution, job control, process inspection,
    and system-call tracing.
2.  **xv6-RISC-V (`xv6-riscv/`)** -- an xv6 kernel modified to implement
    a **4-level Multilevel Feedback Queue (MLFQ)** scheduler, together
    with a custom scheduler test and plotting script.

------------------------------------------------------------------------

## 1. How to Run the Code

### A. C Shell

#### Requirements

The shell is intended to run on a Linux/POSIX environment and requires:

-   GCC
-   GNU Make
-   POSIX/Linux system-call support

The Makefile uses:

-   C23
-   POSIX.1-2008/X/Open interfaces
-   `-Wall -Wextra -Werror`

#### Build

From the project root:

``` bash
cd c-shell
make
```

This creates:

``` text
c-shell/shell.out
```

#### Run

``` bash
./shell.out
```

The shell is interactive, so commands can be entered directly after
starting it.

#### Clean the build

``` bash
make clean
```

This removes the generated `build/` directory and `shell.out`.

#### Examples

Some supported commands/features include:

``` bash
hop
hop ..
hop ~
hop -

reveal
peek file.txt
locate ls

activities

sleep 10 &
activities

ping <pid> <signal>
ping %<job_id> <signal>

resume %<job_id> bg
resume %<job_id> fg
resume %<job_id> fg --timeout <seconds>

spy
spy <pid>

snoop ls
snoop -p <pid>
```

The shell also supports:

``` bash
command < input.txt
command > output.txt
command >> output.txt

command1 | command2
command1 | command2 | command3

command1 ; command2
command1 & command2
```

The lexer supports whitespace separation, `|`, `&`, `;`, `<`, `>`, `>>`,
single quotes, double quotes, and backslash escaping.

------------------------------------------------------------------------

### B. xv6-RISC-V with MLFQ

#### Requirements

The xv6 part requires:

-   RISC-V cross compiler/binutils
-   QEMU system emulator for RISC-V
-   GNU Make
-   Python 3
-   Matplotlib (for generating the scheduler graph)

The Makefile checks for a RISC-V toolchain such as:

``` text
riscv64-unknown-elf-
riscv64-elf-
riscv64-none-elf-
riscv64-linux-gnu-
riscv64-unknown-linux-gnu-
```

It also requires **QEMU 7.2 or newer**.

#### Build and run xv6 with MLFQ

``` bash
cd xv6-riscv
make clean
make qemu SCHEDULER=MLFQ
```

The `SCHEDULER=MLFQ` option enables the `SCHEDULER_MLFQ` compile-time
macro.

Once xv6 boots, run the custom scheduler test:

``` text
schedulertest
```

The test creates four CPU-bound worker processes with different
workloads and prints scheduler trace information.

To exit QEMU, use the standard QEMU/xv6 exit sequence configured for the
environment (commonly `Ctrl-a x`).

#### Run the normal xv6 tests

The repository also contains the standard xv6 testing script:

``` bash
python3 test-xv6.py usertests
```

For the quick usertests:

``` bash
python3 test-xv6.py -q usertests
```

The script can also be used for the existing crash/recovery tests:

``` bash
python3 test-xv6.py crash
python3 test-xv6.py log
python3 test-xv6.py forphan
python3 test-xv6.py dorphan
```

These tests are part of the xv6 support code; the custom MLFQ behaviour
is specifically exercised by `schedulertest`.

------------------------------------------------------------------------

### C. Generate the MLFQ Timeline

The scheduler prints trace lines in the following form:

``` text
MLFQ: tick=<tick> pid=<pid> queue=<queue>
```

To save the QEMU output while running the test, one convenient approach
is:

``` bash
make qemu SCHEDULER=MLFQ | tee mlfq_output.txt
```

Then, inside xv6, run:

``` text
schedulertest
```

After stopping QEMU, make sure `mlfq_output.txt` is present in
`xv6-riscv/`.

Install Matplotlib if necessary:

``` bash
python3 -m pip install matplotlib
```

Then generate the graph:

``` bash
python3 plot_mlfq.py
```

This produces:

``` text
mlfq_timeline.png
```

The plotting script extracts scheduler trace entries for the four worker
PIDs used by the custom scheduler test and plots their queue movement
over time.

------------------------------------------------------------------------

## 2. Folder Structure

The main project structure is:

``` text
mini-project1/
├── README.MD
│
├── c-shell/
│   ├── Makefile
│   ├── include/
│   │   ├── activities.h
│   │   ├── background.h
│   │   ├── execute.h
│   │   ├── hop.h
│   │   ├── lexer.h
│   │   ├── locate.h
│   │   ├── peek.h
│   │   ├── ping.h
│   │   ├── pipeline.h
│   │   ├── prompt.h
│   │   ├── redirection.h
│   │   ├── resume.h
│   │   ├── reveal.h
│   │   ├── sequential.h
│   │   ├── snoop.h
│   │   ├── spy.h
│   │   └── terminal_control.h
│   │
│   └── src/
│       ├── main.c
│       ├── prompt.c
│       ├── lexer.c
│       ├── hop.c
│       ├── reveal.c
│       ├── peek.c
│       ├── locate.c
│       ├── execute.c
│       ├── redirection.c
│       ├── pipeline.c
│       ├── sequential.c
│       ├── background.c
│       ├── activities.c
│       ├── terminal_control.c
│       ├── resume.c
│       ├── ping.c
│       ├── spy.c
│       └── snoop.c
│
└── xv6-riscv/
    ├── Makefile
    ├── kernel/
    │   ├── proc.c
    │   ├── proc.h
    │   ├── trap.c
    │   └── ... standard xv6 kernel files
    │
    ├── user/
    │   ├── schedulertest.c
    │   └── ... standard xv6 user programs
    │
    ├── mkfs/
    │   └── mkfs.c
    │
    ├── test-xv6.py
    ├── plot_mlfq.py
    ├── report.md
    └── mlfq_timeline.png
```

The `build/` directory and `shell.out` are generated by the C-shell
Makefile and therefore do not need to be stored as source files.

------------------------------------------------------------------------

## 3. Design Choices

### 3.1 C Shell

#### Modular source structure

The shell is divided into separate `.c` and `.h` files according to
functionality. This keeps command parsing, execution, job control,
terminal control, and built-in commands independent.

Examples:

-   `lexer.c` -- lexical analysis and syntax validation
-   `execute.c` -- external command execution and command dispatch
-   `pipeline.c` -- pipelines
-   `redirection.c` -- `<`, `>`, and `>>`
-   `sequential.c` -- `;` and `&` command sequencing
-   `background.c` / `activities.c` -- job tracking
-   `terminal_control.c` -- process-group and terminal ownership
-   `resume.c` -- foreground/background resumption
-   `ping.c` -- signal delivery
-   `spy.c` -- process file-descriptor inspection
-   `snoop.c` -- system-call tracing

#### Linked-list token representation

Input is converted into a linked list of tokens. Operators are stored as
dedicated token types while command arguments are stored as word tokens.

This makes it possible for later stages to process pipelines,
redirection, sequencing, and normal arguments without reparsing the
original input string.

#### Fork/exec model

External commands are executed using the standard Unix process model:

1.  Resolve the executable path.
2.  `fork()` a child.
3.  Configure the child process.
4.  Use `execv()` to replace the child with the requested executable.
5.  The parent waits for foreground commands or records background jobs.

#### Process groups and terminal control

Foreground/background execution uses process groups. The shell transfers
terminal control to a foreground process group and takes it back after
the command stops or exits.

This allows signals such as `Ctrl-C` and `Ctrl-Z` to be handled by the
foreground job rather than incorrectly remaining with the shell.

#### Job tracking

Jobs are maintained using dynamically allocated linked structures
containing process-group and process information.

The `activities` command reports tracked jobs and their running/stopped
states.

#### Pipelines

For a pipeline of `N` stages, `N-1` pipes are created. Each stage is
forked into its own child process and its standard input/output is
connected using `dup2()`.

#### Redirection

Input and output redirection is implemented using file descriptors:

-   `<` opens input files read-only.
-   `>` creates/truncates output files.
-   `>>` creates/appends to output files.

Redirection is also processed for individual pipeline stages.

#### `hop` frecency

`hop` maintains a persistent `.hop_history` file in the shell's initial
home directory. Successful directory visits store:

-   absolute path
-   visit count
-   last-visited timestamp

When a direct directory lookup fails, a frecency score is used to select
the best matching previously visited directory.

#### Process inspection and tracing

`spy` uses Linux `/proc/<pid>` information to inspect process-related
file descriptors and paths.

`snoop` uses `ptrace()` to trace system-call entry/exit events and
measure call counts and elapsed time.

------------------------------------------------------------------------

### 3.2 xv6 MLFQ Scheduler

The scheduler uses **four priority queues**:

    Queue  Priority    Time Quantum
  ------- ---------- --------------
       Q0  Highest           1 tick
       Q1                   4 ticks
       Q2                   8 ticks
       Q3   Lowest         16 ticks

Processes start in Q0.

#### Queue ordering

Each process has a `queue_order` value. A monotonically increasing
counter is used to place a process at the tail of its current queue.

Within the same priority level, the runnable process with the earliest
`queue_order` is selected.

This avoids relying only on the physical order of the xv6 process table.

#### Feedback mechanism

A process that consumes its complete time slice is demoted by one queue:

``` text
Q0 → Q1 → Q2 → Q3
```

A process already in Q3 remains in Q3.

The process's `ticks_used` counter is reset when its time slice expires.

#### Voluntary yield

A process that voluntarily yields does **not** get automatically
demoted. Its current priority is preserved, and it is placed at the end
of its current queue.

This distinguishes CPU-bound processes that exhaust their quanta from
processes that give up the CPU voluntarily.

#### Priority boost

Every **48 timer ticks**, a priority boost is marked as pending.

The scheduler then:

-   moves active processes back to Q0
-   resets their `ticks_used`
-   assigns new queue-order values

This prevents lower-priority processes from being starved indefinitely.

#### Higher-priority preemption

Timer-trap handling also checks whether a higher-priority runnable
process exists. A lower-priority running process can therefore yield so
that a higher-priority runnable process gets CPU time.

------------------------------------------------------------------------

## 4. Key xv6 Changes

The main xv6 modifications are:

### `Makefile`

Added scheduler selection through:

``` bash
make qemu SCHEDULER=MLFQ
```

When `SCHEDULER=MLFQ` is supplied, the compiler receives:

``` text
-DSCHEDULER_MLFQ
```

This keeps the MLFQ-specific implementation conditionally compiled.

### `kernel/proc.h`

Added MLFQ state to `struct proc`:

``` c
int priority;
int ticks_used;
uint64 queue_order;
```

### `kernel/proc.c`

Modified:

-   `procinit()`
-   `allocproc()`
-   `userinit()`
-   `kfork()`
-   `scheduler()`
-   `yield()`
-   `wakeup()`
-   `kkill()`
-   `procdump()`

Added the queue-order helper:

``` c
mlfq_put_at_tail()
```

The scheduler scans Q0 through Q3 and selects the earliest queued
runnable process from the highest-priority non-empty queue.

### `kernel/trap.c`

Modified timer-interrupt handling to:

-   count per-process timer ticks
-   enforce Q0/Q1/Q2/Q3 time quanta
-   demote processes after their quanta expire
-   preempt when a higher-priority runnable process is available
-   trigger the periodic boost mechanism

### `user/schedulertest.c`

Added a custom scheduler test with four CPU-bound workers using
different loop limits:

``` text
worker 0 → 500,000,000 iterations
worker 1 → 700,000,000 iterations
worker 2 → 900,000,000 iterations
worker 3 → 1,200,000,000 iterations
```

The different workloads make queue movement and scheduling behaviour
observable.

### `plot_mlfq.py`

Parses scheduler trace lines and generates:

``` text
mlfq_timeline.png
```

The graph uses:

-   X-axis: elapsed timer ticks
-   Y-axis: queue ID (0--3)
-   one series per scheduler-test worker PID

------------------------------------------------------------------------

## 5. Assumptions and Implementation Constraints

The implementation makes the following assumptions:

### C Shell

1.  The shell is intended for a Linux/POSIX environment because it uses
    Linux/POSIX facilities such as `fork()`, `execv()`, `waitpid()`,
    process groups, `/proc`, and `ptrace()`.
2.  The shell input buffer is limited to **4096 characters**.
3.  Temporary argument/token processing uses fixed-size arrays in
    several modules (for example, up to 256 arguments/tokens in relevant
    execution paths).
4.  Pipeline processing supports up to **64 stages** in the pipeline
    representation.
5.  `.hop_history` is stored in the shell's initial working
    directory/home path captured during shell initialization.
6.  `hop -` depends on the shell having a previously recorded working
    directory.
7.  `locate` searches the current directory first and then the
    directories in `PATH`, while the `%` prefix in normal command
    execution skips the current directory and searches `PATH`.
8.  `resume` operates on jobs tracked by the shell and uses `%<job_id>`
    notation.
9.  `ping` accepts numeric signals and reduces the supplied signal
    number modulo 64 before sending it.
10. `spy` relies on `/proc/<pid>` being available.
11. `snoop` relies on Linux `ptrace()` and therefore requires
    appropriate permissions for attaching to another process.
12. On EOF (`Ctrl-D`), the shell warns once if stopped jobs exist; when
    exiting it sends `SIGHUP` to tracked process groups.

### xv6 MLFQ

1.  The scheduler is implemented specifically for **four queues**, Q0
    through Q3.
2.  Q0 has the highest priority.
3.  Time quanta are fixed at 1, 4, 8, and 16 timer ticks.
4.  New processes start in Q0.
5.  Processes are demoted only when they exhaust their current time
    slice.
6.  A voluntary `yield()` preserves the process's current priority.
7.  The priority boost occurs every 48 global timer ticks.
8.  The boost resets active processes to Q0 and resets their accumulated
    slice usage.
9.  Queue ordering is represented using a monotonically increasing
    `uint64` counter.
10. The scheduler trace is printed from the kernel using `printk()`.
11. The custom plotting script is specifically tailored to the four
    workers created by `schedulertest`.
12. The plotting script focuses on the scheduler-test time window and
    worker PIDs used by that test; therefore, the generated graph is
    intended as an analysis of this particular experiment rather than a
    generic visualizer for arbitrary xv6 traces.
13. The standard xv6 build expects a working RISC-V cross-compilation
    toolchain and QEMU. The Makefile enforces QEMU version 7.2 or newer.

------------------------------------------------------------------------

## 6. MLFQ Trace and Analysis

The custom scheduler emits trace information such as:

``` text
MLFQ: tick=... pid=... queue=...
```

The trace can be redirected to `mlfq_output.txt` and processed using
`plot_mlfq.py`.

A process moving from Q0 toward Q1, Q2, and Q3 indicates repeated
consumption of complete CPU quanta. Longer CPU-bound workers are
therefore expected to spend more time in lower-priority queues.

The 48-tick priority boost periodically returns active processes to Q0,
providing an opportunity for lower-priority processes to run at the
highest priority and reducing starvation.

The repository includes the resulting:

``` text
xv6-riscv/mlfq_timeline.png
```
