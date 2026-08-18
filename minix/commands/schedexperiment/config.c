#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "schedexperiment.h"

static char *trim(char *text)
{
	char *end;

	while (isspace((unsigned char)*text))
		text++;
	end = text + strlen(text);
	while (end > text && isspace((unsigned char)end[-1]))
		*--end = '\0';
	return text;
}

static int same(const char *left, const char *right)
{
	while (*left != '\0' && *right != '\0') {
		if (tolower((unsigned char)*left) !=
		    tolower((unsigned char)*right))
			return 0;
		left++;
		right++;
	}
	return *left == '\0' && *right == '\0';
}

static int parse_uint(const char *text, unsigned long *value)
{
	char *end;
	unsigned long parsed;

	errno = 0;
	parsed = strtoul(text, &end, 0);
	if (errno != 0 || end == text || *trim(end) != '\0')
		return -1;
	*value = parsed;
	return 0;
}

static int parse_algorithm(const char *text, enum sched_algorithm *algorithm)
{
	if (same(text, "RR"))
		*algorithm = SCHED_RR;
	else if (same(text, "SJF"))
		*algorithm = SCHED_SJF;
	else if (same(text, "PRIORITY"))
		*algorithm = SCHED_PRIORITY;
	else if (same(text, "MLFQ"))
		*algorithm = SCHED_MLFQ;
	else if (same(text, "ALL"))
		*algorithm = SCHED_ALL;
	else
		return -1;
	return 0;
}

static int parse_mlfq(const char *text, struct sched_config *cfg)
{
	char copy[128];
	char *token;
	unsigned int count;
	unsigned long value;

	if (strlen(text) >= sizeof(copy))
		return -1;
	strcpy(copy, text);
	count = 0;
	for (token = strtok(copy, ","); token != NULL;
	    token = strtok(NULL, ",")) {
		if (count >= SCHED_MLFQ_LEVELS ||
		    parse_uint(trim(token), &value) != 0 || value == 0)
			return -1;
		cfg->mlfq_quantum[count++] = (unsigned int)value;
	}
	return count == SCHED_MLFQ_LEVELS ? 0 : -1;
}

static int parse_job(const char *text, struct sched_config *cfg)
{
	char copy[256];
	char *fields[4];
	char *token;
	unsigned int count;
	unsigned long value;
	struct job_config *job;

	if (cfg->job_count >= SCHED_MAX_JOBS || strlen(text) >= sizeof(copy))
		return -1;
	strcpy(copy, text);
	count = 0;
	for (token = strtok(copy, ","); token != NULL;
	    token = strtok(NULL, ",")) {
		if (count >= 4)
			return -1;
		fields[count++] = trim(token);
	}
	if (count != 4 || fields[0][0] == '\0' ||
	    strlen(fields[0]) >= SCHED_NAME_MAX)
		return -1;
	job = &cfg->jobs[cfg->job_count];
	memset(job, 0, sizeof(*job));
	strcpy(job->name, fields[0]);
	if (parse_uint(fields[1], &value) != 0)
		return -1;
	job->arrival_ms = (unsigned int)value;
	if (parse_uint(fields[2], &value) != 0 || value == 0)
		return -1;
	job->burst_ms = (unsigned int)value;
	if (parse_uint(fields[3], &value) != 0)
		return -1;
	job->priority = (unsigned int)value;
	cfg->job_count++;
	return 0;
}

void sched_config_defaults(struct sched_config *cfg)
{
	memset(cfg, 0, sizeof(*cfg));
	cfg->algorithm = SCHED_ALL;
	cfg->quantum_ms = 20;
	cfg->mlfq_quantum[0] = 10;
	cfg->mlfq_quantum[1] = 20;
	cfg->mlfq_quantum[2] = 40;
	cfg->work_scale = 20000;
	strcpy(cfg->csv_output, "/tmp/schedexperiment.csv");
}

static int set_value(struct sched_config *cfg, const char *key,
	const char *value, char *error, size_t error_size)
{
	unsigned long number;

	if (same(key, "algorithm")) {
		if (parse_algorithm(value, &cfg->algorithm) != 0)
			goto invalid;
	} else if (same(key, "mlfq_quanta_ms")) {
		if (parse_mlfq(value, cfg) != 0)
			goto invalid;
	} else if (same(key, "process")) {
		if (parse_job(value, cfg) != 0)
			goto invalid;
	} else if (same(key, "csv_output")) {
		if (strlen(value) >= sizeof(cfg->csv_output))
			goto invalid;
		strcpy(cfg->csv_output, value);
	} else {
		if (parse_uint(value, &number) != 0)
			goto invalid;
		if (same(key, "quantum_ms"))
			cfg->quantum_ms = (unsigned int)number;
		else if (same(key, "work_scale"))
			cfg->work_scale = number;
		else {
			snprintf(error, error_size,
			    "unknown configuration key: %s", key);
			return -1;
		}
	}
	return 0;

invalid:
	snprintf(error, error_size, "invalid value for %s: %s", key, value);
	return -1;
}

int sched_config_load(const char *path, struct sched_config *cfg,
	char *error, size_t error_size)
{
	FILE *file;
	char line[1024];
	unsigned long line_number;

	file = fopen(path, "r");
	if (file == NULL) {
		snprintf(error, error_size, "cannot open %s: %s", path,
		    strerror(errno));
		return -1;
	}
	line_number = 0;
	while (fgets(line, sizeof(line), file) != NULL) {
		char *key;
		char *value;
		char *separator;
		char *comment;

		line_number++;
		comment = strchr(line, '#');
		if (comment != NULL)
			*comment = '\0';
		key = trim(line);
		if (*key == '\0')
			continue;
		separator = strchr(key, '=');
		if (separator == NULL) {
			snprintf(error, error_size, "%s:%lu: expected key=value",
			    path, line_number);
			fclose(file);
			return -1;
		}
		*separator = '\0';
		value = trim(separator + 1);
		key = trim(key);
		if (set_value(cfg, key, value, error, error_size) != 0) {
			char detail[SCHED_ERROR_MAX];
			strncpy(detail, error, sizeof(detail) - 1);
			detail[sizeof(detail) - 1] = '\0';
			snprintf(error, error_size, "%s:%lu: %s", path,
			    line_number, detail);
			fclose(file);
			return -1;
		}
	}
	fclose(file);
	if (cfg->job_count == 0) {
		snprintf(error, error_size, "configuration contains no processes");
		return -1;
	}
	if (cfg->quantum_ms == 0 || cfg->work_scale == 0) {
		snprintf(error, error_size,
		    "quantum_ms and work_scale must be greater than zero");
		return -1;
	}
	return 0;
}

const char *sched_algorithm_name(enum sched_algorithm algorithm)
{
	if (algorithm == SCHED_RR)
		return "RR";
	if (algorithm == SCHED_SJF)
		return "SJF";
	if (algorithm == SCHED_PRIORITY)
		return "PRIORITY";
	if (algorithm == SCHED_MLFQ)
		return "MLFQ";
	return "ALL";
}
