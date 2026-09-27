2.3.1 Implementation Summary
1. Makefile

File changed: Makefile

The Makefile was updated to accept a scheduler choice through the SCHEDULER option. When the project is compiled using make qemu SCHEDULER=MLFQ, the macro SCHEDULER_MLFQ is defined. This allows the MLFQ-specific implementation to be enabled only when MLFQ is selected.

2. struct proc

File changed: kernel/proc.h
Structure changed: struct proc

Three additional fields were added to maintain the state required by MLFQ:

int priority;
int ticks_used;
uint64 queue_order;

The priority field identifies the queue currently assigned to the process. ticks_used records the number of timer ticks consumed during its current time slice. queue_order is used to maintain the order of runnable processes within the same queue.

3. allocproc()

File changed: kernel/proc.c
Function changed: allocproc()

The newly added MLFQ fields are initialized when a process is allocated:

priority = 0;
ticks_used = 0;
queue_order = 0;

Therefore, every newly created process begins in Q0, the highest-priority queue, with no CPU time accumulated in its current slice.

4. Process Creation: userinit() and kfork()

File changed: kernel/proc.c
Functions changed: userinit(), kfork()

After a newly created process becomes runnable, it is inserted at the end of its current queue using mlfq_put_at_tail(). Since new processes start in Q0, this places them at the end of the highest-priority queue while preserving the ordering between processes.

5. Queue Ordering: mlfq_put_at_tail()

File changed: kernel/proc.c
Function added: mlfq_put_at_tail()

This helper function is responsible for placing a runnable process at the back of its current MLFQ queue. It assigns the process a new queue_order value, allowing processes in the same queue to be selected in the correct order. The function is used when processes are created, become runnable after sleeping, or voluntarily give up the CPU.

6. scheduler()

File changed: kernel/proc.c
Function changed: scheduler()

The MLFQ scheduler was modified to examine the queues according to their priority, starting with Q0 and continuing through Q3. Among the runnable processes in the highest available queue, the process with the earliest queue_order is selected. This provides queue-based priority scheduling while maintaining ordering between processes at the same priority level.

7. yield()

File changed: kernel/proc.c
Function changed: yield()

The yield() function was updated so that voluntarily giving up the CPU does not automatically change a process's priority. Its existing priority and ticks_used values are preserved, while the process is placed at the end of its current queue using mlfq_put_at_tail().

8. wakeup() and kkill()

File changed: kernel/proc.c
Functions changed: wakeup(), kkill()

When a sleeping process becomes runnable again, it is inserted at the end of its current MLFQ queue. This prevents the process from incorrectly jumping ahead of other processes that were already waiting in that queue.

9. usertrap() and kerneltrap()

File changed: kernel/trap.c
Functions changed: usertrap(), kerneltrap()

The timer-interrupt handling was extended to implement MLFQ time slices. The queues use the following time quanta:

Q0: 1 tick
Q1: 4 ticks
Q2: 8 ticks
Q3: 16 ticks

Whenever a process consumes its complete time slice, it is moved to the next lower-priority queue. The timer handling also allows a currently running lower-priority process to be preempted when a higher-priority runnable process is available.

10. clockintr()

File changed: kernel/trap.c
Function changed: clockintr()

A periodic priority-boost mechanism was added to clockintr(). After every 48 timer ticks, a boost is marked as pending. The scheduler then promotes active processes back to Q0 and resets their time-slice usage. This mechanism ensures that processes which have moved to lower queues are not left waiting indefinitely.

11. procdump()

File changed: kernel/proc.c
Function changed: procdump()

procdump() was extended to include MLFQ-specific information when displaying process details. In addition to the normal process information, it reports the process ID, name, state, current queue, and number of ticks used.

12. schedulertest

File changed: user/schedulertest.c
Function: main()

A custom scheduler test was added to create multiple worker processes with different CPU workloads. Since the workers perform different amounts of computation, they consume different amounts of CPU time. This makes it possible to observe the effect of MLFQ scheduling and the movement of processes between its priority queues.

2.3.2 MLFQ Analysis

A custom scheduler test was used to study the behaviour of the MLFQ scheduler. Multiple child processes were created with different CPU workloads so that the scheduler had processes with varying CPU-burst requirements.

The scheduler execution was recorded using trace information containing the elapsed tick, process ID, and queue number. This information was then processed using Python to generate the MLFQ timeline.

Graph file: mlfq_timeline.png
Python file: plot_mlfq.py

The timeline uses elapsed ticks on the X-axis and queue IDs from 0 to 3 on the Y-axis. Different processes are distinguished using different colours. The movement of a process from Q0 towards Q1, Q2, and Q3 indicates that it has consumed its allocated CPU time and has therefore been moved to a lower-priority queue.

The periodic 48-tick priority boost prevents processes that have moved to lower queues from being starved. At a boost, active processes are brought back towards Q0 and their accumulated time-slice usage is reset. Longer-running CPU-bound processes are more likely to appear in the lower queues, while processes that require less CPU time tend to finish earlier.

2.3.3 Comparison Results

The three scheduling approaches considered in this project are FIFO, Round Robin (RR), and Multilevel Feedback Queue (MLFQ). They differ mainly in how they select processes and how they handle CPU time.

FIFO

FIFO is the simplest of the three approaches. Processes are handled according to their arrival order, so an earlier process can continue running before later processes get a chance to execute. This makes the behaviour predictable and easy to implement, but a long CPU-bound process can cause the processes behind it to experience significant waiting time.

Round Robin

Round Robin improves fairness by giving each runnable process a fixed amount of CPU time before moving on to another process. This allows processes to receive CPU time regularly and generally gives better response behaviour than FIFO. However, the selected time quantum affects its performance. A very small quantum can improve responsiveness but may result in more frequent context switches.

MLFQ

MLFQ dynamically changes a process's priority based on how much CPU time it consumes. New processes begin in the highest-priority queue, while processes that repeatedly use their complete time slices are gradually moved to lower-priority queues. This allows short or interactive processes to receive CPU time quickly while longer CPU-bound processes are moved away from the highest-priority queue.

The periodic 48-tick priority boost is an important part of the MLFQ implementation because it gives lower-priority processes another opportunity to run at a high priority. This reduces the possibility of starvation while still allowing the scheduler to favour processes that require quick CPU access.

Overall Comparison

Overall, FIFO has the simplest scheduling behaviour but can suffer from long waiting times when an early process has a large CPU requirement. Round Robin provides better fairness by sharing the processor among runnable processes, making it more suitable when responsiveness is important. MLFQ provides a more adaptive approach by changing process priorities according to CPU usage. It can give short-running processes faster access to the CPU while still allowing CPU-intensive processes to make progress in lower queues. The priority boost further improves fairness by periodically giving lower-priority processes a chance to return to Q0. Therefore, MLFQ provides a good balance between responsiveness, fairness, and efficient CPU sharing, although its implementation is more complex than FIFO and Round Robin.