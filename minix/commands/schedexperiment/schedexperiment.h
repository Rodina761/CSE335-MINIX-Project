#ifndef SCHEDEXPERIMENT_H
#define SCHEDEXPERIMENT_H

#include <stddef.h>
#include <stdint.h>

#define SCHED_MAX_JOBS 32
#define SCHED_NAME_MAX 32
#define SCHED_PATH_MAX 512
#define SCHED_ERROR_MAX 256
#define SCHED_MLFQ_LEVELS 3

enum sched_algorithm {
	SCHED_RR = 0,
	SCHED_SJF = 1,
	SCHED_PRIORITY = 2,
	SCHED_MLFQ = 3,
	SCHED_ALL = 4
};

struct job_config {
	char name[SCHED_NAME_MAX];
	unsigned int arrival_ms;
	unsigned int burst_ms;
	unsigned int priority;
};

struct sched_config {
	enum sched_algorithm algorithm;
	unsigned int quantum_ms;
	unsigned int mlfq_quantum[SCHED_MLFQ_LEVELS];
	unsigned long work_scale;
	char csv_output[SCHED_PATH_MAX];
	struct job_config jobs[SCHED_MAX_JOBS];
	unsigned int job_count;
};

struct job_result {
	char name[SCHED_NAME_MAX];
	unsigned int arrival_ms;
	unsigned int burst_ms;
	unsigned int priority;
	unsigned int start_ms;
	unsigned int completion_ms;
	unsigned int turnaround_ms;
	unsigned int waiting_ms;
	unsigned int response_ms;
};

struct schedule_result {
	enum sched_algorithm algorithm;
	struct job_result jobs[SCHED_MAX_JOBS];
	unsigned int job_count;
	double average_turnaround_ms;
	double average_waiting_ms;
	double average_response_ms;
	unsigned int makespan_ms;
	unsigned int context_switches;
	unsigned long wall_elapsed_ms;
};

void sched_config_defaults(struct sched_config *cfg);
int sched_config_load(const char *path, struct sched_config *cfg,
	char *error, size_t error_size);
const char *sched_algorithm_name(enum sched_algorithm algorithm);
int schedule_run(const struct sched_config *cfg,
	enum sched_algorithm algorithm, struct schedule_result *result,
	char *error, size_t error_size);

#endif
