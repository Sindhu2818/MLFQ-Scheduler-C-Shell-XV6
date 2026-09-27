import re
import matplotlib.pyplot as plt

data = []
boost_ticks = []

with open("mlfq_output.txt", "r") as f:
    for line in f:

        match = re.search(
            r"MLFQ:\s*tick=(\d+)\s+pid=(\d+)\s+queue=(\d+)",
            line
        )

        if match:
            tick = int(match.group(1))
            pid = int(match.group(2))
            queue = int(match.group(3))

            if pid in [4, 5, 6, 7]:
                data.append((tick, pid, queue))

        match = re.search(r"MLFQ BOOST tick=(\d+)", line)

        if match:
            boost_ticks.append(int(match.group(1)))


# Plot workers
pids = [4, 5, 6, 7]

for pid in pids:
    x = []
    y = []

    for tick, process_id, queue in data:
        if process_id == pid:
            x.append(tick)
            y.append(queue)

    plt.scatter(
        x,
        y,
        s=45,
        alpha=0.8,
        label="PID " + str(pid)
    )


# Only show boosts during schedulertest
for boost in boost_ticks:
    if 68 <= boost <= 117:
        plt.axvline(
            x=boost,
            linestyle="--",
            linewidth=1.5,
            label="48-tick boost"
        )


plt.xlabel("Time elapsed (ticks)")
plt.ylabel("Queue ID")
plt.title("MLFQ Scheduling Timeline")

plt.yticks([0, 1, 2, 3])
plt.ylim(-0.3, 3.3)

# IMPORTANT: focus on schedulertest
plt.xlim(65, 120)
plt.xticks(range(70, 121, 5))

plt.legend()
plt.grid(True, alpha=0.3)
plt.tight_layout()

plt.savefig("mlfq_timeline.png", dpi=300)
plt.show()