#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "schedexperiment.h"

struct child_command {
	unsigned int run_ms;
};

struct child_reply {
	unsigned int completed_ms;
};

struct job_state {
	const struct job_config *cfg;
	pid_t pid;
	int command_fd;
	int reply_fd;
	unsigned int remaining_ms;
	unsigned int start_ms;
	unsigned int completion_ms;
	unsigned int queue_level;
	unsigned long queue_stamp;
	int started;
	int finished;
};

static int write_full(int fd, const void *buffer, size_t size)
{
	const char *bytes;
	size_t done;
	ssize_t count;

	bytes = buffer;
	done = 0;
	while (done < size) {
		count = write(fd, bytes + done, size - done);
		if (count < 0 && errno == EINTR)
			continue;
		if (count <= 0)
			return -1;
		done += (size_t)count;
	}
	return 0;
}

static int read_full(int fd, void *buffer, size_t size)
{
	char *bytes;
	size_t done;
	ssize_t count;

	bytes = buffer;
	done = 0;
	while (done < size) {
		count = read(fd, bytes + done, size - done);
		if (count < 0 && errno == EINTR)
			continue;
		if (count <= 0)
			return -1;
		done += (size_t)count;
	}
	return 0;
}

static void perform_work(unsigned int run_ms, unsigned long scale)
{
	volatile unsigned long value;
	unsigned long outer;
	unsigned long inner;

	value = 0x335UL;
	for (outer = 0; outer < run_ms; outer++) {
		for (inner = 0; inner < scale; inner++)
			value = value * 1664525UL + 1013904223UL;
	}
	(void)value;
}

static void child_main(int command_fd, int reply_fd, unsigned int burst_ms,
	unsigned long work_scale)
{
	struct child_command command;
	struct child_reply reply;
	unsigned int completed;

	completed = 0;
	while (completed < burst_ms) {
		if (read_full(command_fd, &command, sizeof(command)) != 0)
			_exit(2);
		if (command.run_ms == 0 ||
		    command.run_ms > burst_ms - completed)
			_exit(3);
		perform_work(command.run_ms, work_scale);
		completed += command.run_ms;
		reply.completed_ms = command.run_ms;
		if (write_full(reply_fd, &reply, sizeof(reply)) != 0)
			_exit(4);
	}
	_exit(0);
}

static void close_children(struct job_state *states, unsigned int count,
	int terminate)
{
	unsigned int index;

	for (index = 0; index < count; index++) {
		if (states[index].command_fd >= 0)
			close(states[index].command_fd);
		if (states[index].reply_fd >= 0)
			close(states[index].reply_fd);
		if (terminate && states[index].pid > 0)
			kill(states[index].pid, SIGKILL);
	}
	for (index = 0; index < count; index++) {
		if (states[index].pid > 0)
			(void)waitpid(states[index].pid, NULL, 0);
	}
}

static int create_children(const struct sched_config *cfg,
	struct job_state *states, char *error, size_t error_size)
{
	unsigned int index;

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
	return 0;
}

static int ready(const struct job_state *state, unsigned int now)
{
	return !state->finished && state->cfg->arrival_ms <= now;
}

static int next_arrival(const struct job_state *states, unsigned int count,
	unsigned int now)
{
	unsigned int index;
	unsigned int selected;
	int found;

	selected = 0;
	found = 0;
	for (index = 0; index < count; index++) {
		if (!states[index].finished && states[index].cfg->arrival_ms > now &&
		    (!found || states[index].cfg->arrival_ms < selected)) {
			selected = states[index].cfg->arrival_ms;
			found = 1;
		}
	}
	return found ? (int)selected : -1;
}

static int select_rr(const struct job_state *states, unsigned int count,
	unsigned int now, unsigned int *cursor)
{
	unsigned int offset;
	unsigned int index;

	for (offset = 0; offset < count; offset++) {
		index = (*cursor + offset) % count;
		if (ready(&states[index], now)) {
			*cursor = (index + 1) % count;
			return (int)index;
		}
	}
	return -1;
}

static int select_sjf(const struct job_state *states, unsigned int count,
	unsigned int now)
{
	unsigned int index;
	int selected;

	selected = -1;
	for (index = 0; index < count; index++) {
		if (ready(&states[index], now) && (selected < 0 ||
		    states[index].remaining_ms < states[selected].remaining_ms ||
		    (states[index].remaining_ms == states[selected].remaining_ms &&
		    states[index].cfg->arrival_ms < states[selected].cfg->arrival_ms)))
			selected = (int)index;
	}
	return selected;
}

static int select_priority(const struct job_state *states,
	unsigned int count, unsigned int now)
{
	unsigned int index;
	int selected;

	selected = -1;
	for (index = 0; index < count; index++) {
		if (ready(&states[index], now) && (selected < 0 ||
		    states[index].cfg->priority < states[selected].cfg->priority ||
		    (states[index].cfg->priority == states[selected].cfg->priority &&
		    states[index].cfg->arrival_ms < states[selected].cfg->arrival_ms)))
			selected = (int)index;
	}
	return selected;
}

static int select_mlfq(const struct job_state *states, unsigned int count,
	unsigned int now)
{
	unsigned int index;
	int selected;

	selected = -1;
	for (index = 0; index < count; index++) {
		if (ready(&states[index], now) && (selected < 0 ||
		    states[index].queue_level < states[selected].queue_level ||
		    (states[index].queue_level == states[selected].queue_level &&
		    states[index].queue_stamp < states[selected].queue_stamp)))
			selected = (int)index;
	}
	return selected;
}

static unsigned long elapsed_ms(const struct timeval *start,
	const struct timeval *end)
{
	long seconds;
	long microseconds;

	seconds = end->tv_sec - start->tv_sec;
	microseconds = end->tv_usec - start->tv_usec;
	return (unsigned long)(seconds * 1000L + microseconds / 1000L);
}

int schedule_run(const struct sched_config *cfg,
	enum sched_algorithm algorithm, struct schedule_result *result,
	char *error, size_t error_size)
{
	struct job_state states[SCHED_MAX_JOBS];
	struct timeval wall_start;
	struct timeval wall_end;
	unsigned int completed;
	unsigned int now;
	unsigned int cursor;
	unsigned long stamp;
	int selected;
	unsigned int index;

	memset(states, 0, sizeof(states));
	memset(result, 0, sizeof(*result));
	result->algorithm = algorithm;
	result->job_count = cfg->job_count;
	if (create_children(cfg, states, error, error_size) != 0)
		return -1;
	gettimeofday(&wall_start, NULL);
	completed = 0;
	now = 0;
	cursor = 0;
	stamp = cfg->job_count;
	while (completed < cfg->job_count) {
		unsigned int slice;
		struct child_command command;
		struct child_reply reply;
		int status;

		if (algorithm == SCHED_RR)
			selected = select_rr(states, cfg->job_count, now, &cursor);
		else if (algorithm == SCHED_SJF)
			selected = select_sjf(states, cfg->job_count, now);
		else if (algorithm == SCHED_PRIORITY)
			selected = select_priority(states, cfg->job_count, now);
		else
			selected = select_mlfq(states, cfg->job_count, now);
		if (selected < 0) {
			selected = next_arrival(states, cfg->job_count, now);
			if (selected < 0) {
				snprintf(error, error_size, "scheduler reached invalid idle state");
				close_children(states, cfg->job_count, 1);
				return -1;
			}
			now = (unsigned int)selected;
			continue;
		}
		if (!states[selected].started) {
			states[selected].started = 1;
			states[selected].start_ms = now;
		}
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
		if (states[selected].remaining_ms == 0) {
			states[selected].finished = 1;
			states[selected].completion_ms = now;
			close(states[selected].command_fd);
			close(states[selected].reply_fd);
			states[selected].command_fd = -1;
			states[selected].reply_fd = -1;
			if (waitpid(states[selected].pid, &status, 0) < 0 ||
			    !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
				snprintf(error, error_size, "worker %s exited abnormally",
				    states[selected].cfg->name);
				states[selected].pid = -1;
				close_children(states, cfg->job_count, 1);
				return -1;
			}
			states[selected].pid = -1;
			completed++;
		} else if (algorithm == SCHED_MLFQ) {
			if (states[selected].queue_level + 1 < SCHED_MLFQ_LEVELS)
				states[selected].queue_level++;
			states[selected].queue_stamp = stamp++;
		}
	}
	gettimeofday(&wall_end, NULL);
	result->makespan_ms = now;
	result->wall_elapsed_ms = elapsed_ms(&wall_start, &wall_end);
	for (index = 0; index < cfg->job_count; index++) {
		struct job_result *job;

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
	}
	result->average_turnaround_ms /= cfg->job_count;
	result->average_waiting_ms /= cfg->job_count;
	result->average_response_ms /= cfg->job_count;
	close_children(states, cfg->job_count, 0);
	return 0;
}
