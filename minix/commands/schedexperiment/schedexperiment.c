#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "schedexperiment.h"

static void usage(const char *program)
{
	fprintf(stderr, "Usage: %s [-c config] [-o csv-output]\n", program);
}

static void print_result(const struct schedule_result *result)
{
	unsigned int index;

	printf("%s: avg turnaround %.2f ms, avg waiting %.2f ms, ",
	    sched_algorithm_name(result->algorithm),
	    result->average_turnaround_ms, result->average_waiting_ms);
	printf("avg response %.2f ms, makespan %u ms, switches %u\n",
	    result->average_response_ms, result->makespan_ms,
	    result->context_switches);
	for (index = 0; index < result->job_count; index++) {
		const struct job_result *job;

		job = &result->jobs[index];
		printf("  %-12s start=%u complete=%u turnaround=%u waiting=%u\n",
		    job->name, job->start_ms, job->completion_ms,
		    job->turnaround_ms, job->waiting_ms);
	}
}

static int write_csv(const char *path, const struct schedule_result *results,
	unsigned int result_count)
{
	FILE *file;
	unsigned int result_index;

	file = fopen(path, "w");
	if (file == NULL) {
		fprintf(stderr, "schedexperiment: cannot write %s: %s\n",
		    path, strerror(errno));
		return -1;
	}
	fprintf(file, "algorithm,process,arrival_ms,burst_ms,priority,start_ms,");
	fprintf(file, "completion_ms,turnaround_ms,waiting_ms,response_ms,");
	fprintf(file, "average_turnaround_ms,average_waiting_ms,");
	fprintf(file, "average_response_ms,makespan_ms,context_switches,");
	fprintf(file, "wall_elapsed_ms\n");
	for (result_index = 0; result_index < result_count; result_index++) {
		const struct schedule_result *result;
		unsigned int job_index;

		result = &results[result_index];
		for (job_index = 0; job_index < result->job_count; job_index++) {
			const struct job_result *job;

			job = &result->jobs[job_index];
			fprintf(file, "%s,%s,%u,%u,%u,%u,%u,%u,%u,%u,",
			    sched_algorithm_name(result->algorithm), job->name,
			    job->arrival_ms, job->burst_ms, job->priority,
			    job->start_ms, job->completion_ms, job->turnaround_ms,
			    job->waiting_ms, job->response_ms);
			fprintf(file, "%.2f,%.2f,%.2f,%u,%u,%lu\n",
			    result->average_turnaround_ms,
			    result->average_waiting_ms,
			    result->average_response_ms, result->makespan_ms,
			    result->context_switches, result->wall_elapsed_ms);
		}
	}
	return fclose(file) == 0 ? 0 : -1;
}

int main(int argc, char **argv)
{
	const char *config_path;
	const char *output_override;
	struct sched_config cfg;
	struct schedule_result results[4];
	unsigned int result_count;
	char error[SCHED_ERROR_MAX];
	int argument;
	int first;
	int last;
	int algorithm;

	config_path = "/etc/scheduler.conf";
	output_override = NULL;
	for (argument = 1; argument < argc; argument++) {
		if (strcmp(argv[argument], "-c") == 0 && argument + 1 < argc)
			config_path = argv[++argument];
		else if (strcmp(argv[argument], "-o") == 0 && argument + 1 < argc)
			output_override = argv[++argument];
		else {
			usage(argv[0]);
			return 2;
		}
	}
	sched_config_defaults(&cfg);
	if (sched_config_load(config_path, &cfg, error, sizeof(error)) != 0) {
		fprintf(stderr, "schedexperiment: %s\n", error);
		return 1;
	}
	if (output_override != NULL) {
		if (strlen(output_override) >= sizeof(cfg.csv_output)) {
			fprintf(stderr, "schedexperiment: output path is too long\n");
			return 1;
		}
		strcpy(cfg.csv_output, output_override);
	}
	if (cfg.algorithm == SCHED_ALL) {
		first = SCHED_RR;
		last = SCHED_MLFQ;
	} else {
		first = cfg.algorithm;
		last = cfg.algorithm;
	}
	result_count = 0;
	for (algorithm = first; algorithm <= last; algorithm++) {
		if (schedule_run(&cfg, (enum sched_algorithm)algorithm,
		    &results[result_count], error, sizeof(error)) != 0) {
			fprintf(stderr, "schedexperiment: %s\n", error);
			return 1;
		}
		print_result(&results[result_count]);
		result_count++;
	}
	if (write_csv(cfg.csv_output, results, result_count) != 0) {
		fprintf(stderr, "schedexperiment: failed to write CSV\n");
		return 1;
	}
	printf("CSV results written to %s\n", cfg.csv_output);
	return 0;
}
