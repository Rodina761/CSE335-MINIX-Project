# Requirement 1 — CPU scheduling discussion guide

## What the specification asks for

Implement Round Robin, Shortest Job First, priority-based scheduling, and a
Multi-Level Feedback Queue in MINIX. Parameters must be editable in a
configuration file. Real processes must execute, and average turnaround and
waiting times must be measured and compared.

## 30-second explanation

`schedexperiment` runs inside MINIX and creates one real child process for every
configured job. The parent implements the four policy selectors and controls
which child executes a logical CPU slice through pipes. Each child performs
real deterministic CPU work and replies when its slice finishes. Logical time
makes the policy metrics reproducible, while wall time proves that actual work
ran. Results are written per process to CSV.

## Architecture and code map

| File | Responsibility |
|---|---|
| `etc/scheduler.conf` | Algorithm, quanta, work scale, output, and workload |
| `minix/commands/schedexperiment/config.c` | Parses and validates configuration |
| `minix/commands/schedexperiment/scheduler.c` | Creates workers, selects jobs, dispatches slices, calculates results |
| `minix/commands/schedexperiment/schedexperiment.c` | Runs requested policies and writes CSV |
| `minix/commands/schedexperiment/tests/` | Known workload and exact-answer test |
| `minix/commands/schedexperiment/run_schedexperiments.sh` | Quantum experiment matrix |

Important functions in `scheduler.c`:

- `create_children()` creates the real worker processes and pipes.
- `select_rr()` uses a rotating cursor among ready jobs.
- `select_sjf()` chooses the ready job with the smallest remaining burst.
- `select_priority()` chooses the smallest numeric priority.
- `select_mlfq()` chooses the oldest ready job in the highest available queue.
- `schedule_run()` is the main dispatch loop and metric collector.

## Exact code to study

These are the source blocks most likely to be opened during the discussion.
Line numbers refer to the submitted commit.

### 1. Real process creation — `scheduler.c:134-182`

```c
for (index = 0; index < cfg->job_count; index++) {
	int command_pipe[2];
	int reply_pipe[2];
	pid_t pid;

	states[index].cfg = &cfg->jobs[index];
	states[index].remaining_ms = cfg->jobs[index].burst_ms;
	states[index].command_fd = -1;
	states[index].reply_fd = -1;
	states[index].queue_stamp = index;
	if (pipe(command_pipe) != 0) {
		snprintf(error, error_size, "pipe failed: %s", strerror(errno));
		close_children(states, index, 1);
		return -1;
	}
	if (pipe(reply_pipe) != 0) {
		snprintf(error, error_size, "pipe failed: %s", strerror(errno));
		close(command_pipe[0]);
		close(command_pipe[1]);
		close_children(states, index, 1);
		return -1;
	}
	pid = fork();
	if (pid < 0) {
		snprintf(error, error_size, "fork failed: %s", strerror(errno));
		close(command_pipe[0]);
		close(command_pipe[1]);
		close(reply_pipe[0]);
		close(reply_pipe[1]);
		close_children(states, index, 1);
		return -1;
	}
	if (pid == 0) {
		unsigned int prior;

		close(command_pipe[1]);
		close(reply_pipe[0]);
		for (prior = 0; prior < index; prior++) {
			close(states[prior].command_fd);
			close(states[prior].reply_fd);
		}
		child_main(command_pipe[0], reply_pipe[1],
		    cfg->jobs[index].burst_ms, cfg->work_scale);
	}
	close(command_pipe[0]);
	close(reply_pipe[1]);
	states[index].pid = pid;
	states[index].command_fd = command_pipe[1];
	states[index].reply_fd = reply_pipe[0];
}
```

What it proves: there is one real `fork()` per configured process and two pipes
for parent-to-child commands and child-to-parent acknowledgements.

Likely question: **Why close unused pipe ends?** To avoid descriptor leaks and,
more importantly, to ensure EOF and blocking behavior work correctly; an extra
open writer could prevent a reader from ever seeing EOF.

### 2. The four selectors — `scheduler.c:211-275`

```c
/* RR: rotate the cursor after selecting a ready process. */
for (offset = 0; offset < count; offset++) {
	index = (*cursor + offset) % count;
	if (ready(&states[index], now)) {
		*cursor = (index + 1) % count;
		return (int)index;
	}
}

/* SJF: minimum remaining burst, then earlier arrival. */
if (ready(&states[index], now) && (selected < 0 ||
    states[index].remaining_ms < states[selected].remaining_ms ||
    (states[index].remaining_ms == states[selected].remaining_ms &&
    states[index].cfg->arrival_ms < states[selected].cfg->arrival_ms)))
	selected = (int)index;

/* Priority: smaller numeric priority wins, then earlier arrival. */
if (ready(&states[index], now) && (selected < 0 ||
    states[index].cfg->priority < states[selected].cfg->priority ||
    (states[index].cfg->priority == states[selected].cfg->priority &&
    states[index].cfg->arrival_ms < states[selected].cfg->arrival_ms)))
	selected = (int)index;

/* MLFQ: highest queue first, then oldest queue stamp. */
if (ready(&states[index], now) && (selected < 0 ||
    states[index].queue_level < states[selected].queue_level ||
    (states[index].queue_level == states[selected].queue_level &&
    states[index].queue_stamp < states[selected].queue_stamp)))
	selected = (int)index;
```

These are exact decision expressions from the four selector functions. The
repeated `ready()` condition prevents a process from running before arrival.

Likely question: **How are ties made deterministic?** SJF and priority use
arrival time; MLFQ uses a monotonically increasing queue stamp; RR uses the
rotating cursor.

### 3. Dispatch, slice selection, and MLFQ demotion — `scheduler.c:320-383`

```c
if (algorithm == SCHED_RR)
	selected = select_rr(states, cfg->job_count, now, &cursor);
else if (algorithm == SCHED_SJF)
	selected = select_sjf(states, cfg->job_count, now);
else if (algorithm == SCHED_PRIORITY)
	selected = select_priority(states, cfg->job_count, now);
else
	selected = select_mlfq(states, cfg->job_count, now);

if (algorithm == SCHED_SJF || algorithm == SCHED_PRIORITY)
	slice = states[selected].remaining_ms;
else if (algorithm == SCHED_RR)
	slice = cfg->quantum_ms;
else
	slice = cfg->mlfq_quantum[states[selected].queue_level];
if (slice > states[selected].remaining_ms)
	slice = states[selected].remaining_ms;

command.run_ms = slice;
if (write_full(states[selected].command_fd, &command,
    sizeof(command)) != 0 ||
    read_full(states[selected].reply_fd, &reply, sizeof(reply)) != 0 ||
    reply.completed_ms != slice) {
	snprintf(error, error_size, "worker %s failed",
	    states[selected].cfg->name);
	close_children(states, cfg->job_count, 1);
	return -1;
}

now += slice;
states[selected].remaining_ms -= slice;
result->context_switches++;

/* Executed when an MLFQ job still has remaining work. */
if (states[selected].queue_level + 1 < SCHED_MLFQ_LEVELS)
	states[selected].queue_level++;
states[selected].queue_stamp = stamp++;
```

Likely question: **Why do SJF and priority run the entire remainder?** Because
the implemented versions are explicitly non-preemptive. RR and MLFQ use quanta.

### 4. Metric calculation — `scheduler.c:392-408`

```c
job = &result->jobs[index];
strcpy(job->name, cfg->jobs[index].name);
job->arrival_ms = cfg->jobs[index].arrival_ms;
job->burst_ms = cfg->jobs[index].burst_ms;
job->priority = cfg->jobs[index].priority;
job->start_ms = states[index].start_ms;
job->completion_ms = states[index].completion_ms;
job->turnaround_ms = job->completion_ms - job->arrival_ms;
job->waiting_ms = job->turnaround_ms - job->burst_ms;
job->response_ms = job->start_ms - job->arrival_ms;
result->average_turnaround_ms += job->turnaround_ms;
result->average_waiting_ms += job->waiting_ms;
result->average_response_ms += job->response_ms;

result->average_turnaround_ms /= cfg->job_count;
result->average_waiting_ms /= cfg->job_count;
result->average_response_ms /= cfg->job_count;
```

Likely question: **Why is waiting `turnaround - burst`?** Turnaround contains
both time executing and time not executing; subtracting total CPU burst leaves
the logical time spent waiting.

## Configuration to know

```ini
algorithm=ALL
quantum_ms=20
mlfq_quanta_ms=10,20,40
work_scale=20000
csv_output=/tmp/schedexperiment.csv
process=P1,0,80,3
```

Each process row is `name, arrival time, burst time, priority`. A smaller
numeric priority means higher priority. `work_scale` controls how much integer
work represents one logical burst millisecond; it does not change logical
scheduling results.

## How each algorithm works here

### Round Robin

RR selects ready processes in cyclic order and runs each for at most
`quantum_ms`. If the remaining burst is smaller than the quantum, it runs only
the remainder. A small quantum improves response and fairness but causes more
dispatches. A very large quantum approaches FCFS behavior.

### Shortest Job First

The implementation is non-preemptive. From the jobs that have already arrived,
it selects the smallest remaining burst and runs it to completion. SJF often
minimizes mean waiting time when burst lengths are known, but long jobs can wait
and real systems usually do not know future CPU bursts exactly.

### Static priority

The implementation is non-preemptive. It selects the ready process with the
smallest numeric priority and runs it to completion. It expresses importance,
but low-priority jobs can starve because this experiment does not implement
aging.

### MLFQ

New jobs begin in queue 0. Queue 0 has the smallest quantum, and lower queues
have larger quanta. An unfinished process is demoted after its slice. This gives
short or interactive-looking jobs quick service without requiring a supplied
burst estimate. The implementation has three queues and no periodic priority
boost, which is a limitation to mention.

## Metrics and formulas

For each process:

```text
turnaround = completion - arrival
waiting    = turnaround - burst
response   = first_start - arrival
```

The command also records average values, logical makespan, dispatch count, and
measured wall elapsed time. The CSV calls dispatches a context-switch proxy;
they are not exact kernel context-switch counters.

## Native known-workload result

| Algorithm | Avg turnaround | Avg waiting | Avg response | Makespan | Dispatches |
|---|---:|---:|---:|---:|---:|
| RR | 185 ms | 133 ms | 17 ms | 260 ms | 14 |
| SJF | 137 ms | 85 ms | 85 ms | 260 ms | 5 |
| Priority | 155 ms | 103 ms | 103 ms | 260 ms | 5 |
| MLFQ | 191 ms | 139 ms | 5 ms | 260 ms | 15 |

Interpretation: SJF achieved the lowest average waiting and turnaround on this
workload. MLFQ gave the fastest initial response but needed the most dispatches.
The makespan is always 260 ms because all policies eventually execute the same
total burst and there is no final idle gap.

## Demo

```sh
sed -n '1,100p' /etc/scheduler.conf
schedexperiment -c /etc/scheduler.conf
head -8 /tmp/schedexperiment.csv

cd /usr/src/minix/commands/schedexperiment/tests
sh test_known.sh
```

Expected final line: `PASS: known scheduling workload`.

To demonstrate configurability, change only `quantum_ms`, rerun RR, and compare
dispatch count and response time. Restore the original file afterward.

## Likely questions and model answers

### Are these real processes?

Yes. The parent calls `fork()` once per job. Children block on command pipes,
perform CPU work when dispatched, and acknowledge completion. The policy clock
is logical for reproducibility, but the workers are real MINIX processes.

### Did you replace the production MINIX kernel scheduler?

No. This is a controlled dispatcher compiled and executed inside MINIX. It
satisfies real-process execution and policy comparison, but it is not a kernel
scheduler replacement. State this boundary honestly.

### Why use logical time?

VM load and host scheduling make wall time noisy. Logical time makes turnaround,
waiting, and response exactly repeatable for a defined workload. Wall time is
still recorded to confirm actual work executed.

### Is your SJF preemptive?

No. It is non-preemptive SJF. The preemptive form would be Shortest Remaining
Time First and would re-evaluate when a new process arrives.

### How are ties resolved?

Deterministically, using arrival/order information rather than arbitrary array
behavior. This ensures identical configuration produces identical logical
results.

### Why can MLFQ have worse average waiting than SJF?

MLFQ optimizes responsiveness without knowing future bursts. It repeatedly
serves and demotes jobs, which improves first response but may delay completion
and increase dispatch overhead. SJF has the unfair advantage of known bursts.

### What happens when no job is ready?

The logical clock jumps to the next configured arrival instead of dispatching a
future process or busy-waiting.

## Claims to avoid

- Do not call the dispatch count an exact kernel context-switch count.
- Do not claim the production MINIX scheduler was replaced.
- Do not call SJF or priority preemptive.
- Do not say one algorithm is universally best; the result is workload-specific.
