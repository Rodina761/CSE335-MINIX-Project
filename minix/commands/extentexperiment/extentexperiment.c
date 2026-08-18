#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "extentexperiment.h"

static unsigned long elapsed_us(const struct timeval *start,
	const struct timeval *end)
{
	long seconds;
	long microseconds;

	seconds = end->tv_sec - start->tv_sec;
	microseconds = end->tv_usec - start->tv_usec;
	return (unsigned long)(seconds * 1000000L + microseconds);
}

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

static void fill_pattern(unsigned char *buffer, size_t size,
	unsigned long long offset, unsigned int iteration)
{
	size_t index;

	for (index = 0; index < size; index++)
		buffer[index] = (unsigned char)((offset + index + iteration) & 0xff);
}

static unsigned int verify_pattern(const unsigned char *buffer, size_t size,
	unsigned long long offset, unsigned int iteration)
{
	size_t index;
	unsigned int errors;

	errors = 0;
	for (index = 0; index < size; index++) {
		if (buffer[index] !=
		    (unsigned char)((offset + index + iteration) & 0xff))
			errors++;
	}
	return errors;
}

static double throughput_mib(unsigned long long bytes, unsigned long usec)
{
	if (usec == 0)
		return 0.0;
	return ((double)bytes / (1024.0 * 1024.0)) /
	    ((double)usec / 1000000.0);
}

static int run_iteration(const struct extent_config *cfg, unsigned int iteration,
	FILE *csv, char *error, size_t error_size)
{
	char subdir[EXTENT_PATH_MAX];
	char filepath[EXTENT_PATH_MAX];
	unsigned char *buffer;
	size_t chunk_size;
	unsigned long long total_bytes;
	unsigned long long offset;
	struct timeval create_start, create_end;
	struct timeval write_start, write_end;
	struct timeval read_start, read_end;
	struct timeval remove_start, remove_end;
	struct stat info;
	unsigned long create_us, write_us, read_us, remove_us;
	unsigned int verify_errors;
	int fd;

	if (snprintf(subdir, sizeof(subdir), "%s/run-%u", cfg->directory,
	    iteration) >= (int)sizeof(subdir) ||
	    snprintf(filepath, sizeof(filepath), "%s/data.bin", subdir) >=
	    (int)sizeof(filepath)) {
		snprintf(error, error_size, "benchmark path is too long");
		return -1;
	}
	chunk_size = (size_t)cfg->extent_blocks * cfg->block_size;
	total_bytes = (unsigned long long)cfg->file_blocks * cfg->block_size;
	buffer = malloc(chunk_size);
	if (buffer == NULL) {
		snprintf(error, error_size, "cannot allocate extent buffer");
		return -1;
	}
	gettimeofday(&create_start, NULL);
	if (mkdir(subdir, 0755) != 0) {
		snprintf(error, error_size, "mkdir %s failed: %s", subdir,
		    strerror(errno));
		free(buffer);
		return -1;
	}
	fd = open(filepath, O_CREAT | O_TRUNC | O_RDWR, 0644);
	gettimeofday(&create_end, NULL);
	if (fd < 0) {
		snprintf(error, error_size, "open %s failed: %s", filepath,
		    strerror(errno));
		rmdir(subdir);
		free(buffer);
		return -1;
	}
	gettimeofday(&write_start, NULL);
	for (offset = 0; offset < total_bytes; offset += chunk_size) {
		size_t amount;

		amount = chunk_size;
		if ((unsigned long long)amount > total_bytes - offset)
			amount = (size_t)(total_bytes - offset);
		fill_pattern(buffer, amount, offset, iteration);
		if (write_full(fd, buffer, amount) != 0) {
			snprintf(error, error_size, "write failed: %s", strerror(errno));
			close(fd);
			unlink(filepath);
			rmdir(subdir);
			free(buffer);
			return -1;
		}
	}
	if (fsync(fd) != 0) {
		snprintf(error, error_size, "fsync failed: %s", strerror(errno));
		close(fd);
		unlink(filepath);
		rmdir(subdir);
		free(buffer);
		return -1;
	}
	gettimeofday(&write_end, NULL);
	if (fstat(fd, &info) != 0 || lseek(fd, 0, SEEK_SET) < 0) {
		snprintf(error, error_size, "stat/seek failed: %s", strerror(errno));
		close(fd);
		unlink(filepath);
		rmdir(subdir);
		free(buffer);
		return -1;
	}
	verify_errors = 0;
	gettimeofday(&read_start, NULL);
	for (offset = 0; offset < total_bytes; offset += chunk_size) {
		size_t amount;

		amount = chunk_size;
		if ((unsigned long long)amount > total_bytes - offset)
			amount = (size_t)(total_bytes - offset);
		if (read_full(fd, buffer, amount) != 0) {
			snprintf(error, error_size, "read failed: %s", strerror(errno));
			close(fd);
			unlink(filepath);
			rmdir(subdir);
			free(buffer);
			return -1;
		}
		verify_errors += verify_pattern(buffer, amount, offset, iteration);
	}
	gettimeofday(&read_end, NULL);
	close(fd);
	gettimeofday(&remove_start, NULL);
	if (unlink(filepath) != 0 || rmdir(subdir) != 0) {
		snprintf(error, error_size, "file/directory removal failed: %s",
		    strerror(errno));
		free(buffer);
		return -1;
	}
	gettimeofday(&remove_end, NULL);
	free(buffer);
	create_us = elapsed_us(&create_start, &create_end);
	write_us = elapsed_us(&write_start, &write_end);
	read_us = elapsed_us(&read_start, &read_end);
	remove_us = elapsed_us(&remove_start, &remove_end);
	fprintf(csv, "%u,%u,%u,%u,%llu,%u,%lld,%lu,%lu,%lu,%lu,",
	    cfg->extent_blocks, iteration, cfg->block_size, cfg->file_blocks,
	    total_bytes,
	    (cfg->file_blocks + cfg->extent_blocks - 1) / cfg->extent_blocks,
	    (long long)info.st_blocks, create_us, write_us, read_us, remove_us);
	fprintf(csv, "%.3f,%.3f,%u\n", throughput_mib(total_bytes, write_us),
	    throughput_mib(total_bytes, read_us), verify_errors);
	printf("extent=%u run=%u write=%.3f MiB/s read=%.3f MiB/s errors=%u\n",
	    cfg->extent_blocks, iteration, throughput_mib(total_bytes, write_us),
	    throughput_mib(total_bytes, read_us), verify_errors);
	if (verify_errors != 0) {
		snprintf(error, error_size, "%u data verification errors",
		    verify_errors);
		return -1;
	}
	return 0;
}

static void usage(const char *program)
{
	fprintf(stderr, "Usage: %s [-c config] [-o csv-output]\n", program);
}

int main(int argc, char **argv)
{
	const char *config_path;
	const char *output_override;
	struct extent_config cfg;
	char error[EXTENT_ERROR_MAX];
	FILE *csv;
	unsigned int iteration;
	int argument;

	config_path = "/etc/extent.conf";
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
	extent_config_defaults(&cfg);
	if (extent_config_load(config_path, &cfg, error, sizeof(error)) != 0) {
		fprintf(stderr, "extentexperiment: %s\n", error);
		return 1;
	}
	if (output_override != NULL) {
		if (strlen(output_override) >= sizeof(cfg.csv_output)) {
			fprintf(stderr, "extentexperiment: output path is too long\n");
			return 1;
		}
		strcpy(cfg.csv_output, output_override);
	}
	if (mkdir(cfg.directory, 0755) != 0 && errno != EEXIST) {
		fprintf(stderr, "extentexperiment: mkdir %s failed: %s\n",
		    cfg.directory, strerror(errno));
		return 1;
	}
	csv = fopen(cfg.csv_output, "w");
	if (csv == NULL) {
		fprintf(stderr, "extentexperiment: cannot write %s: %s\n",
		    cfg.csv_output, strerror(errno));
		return 1;
	}
	fprintf(csv, "extent_blocks,iteration,block_size,file_blocks,bytes,");
	fprintf(csv, "logical_extents,allocated_512_blocks,create_us,write_us,");
	fprintf(csv, "read_us,remove_us,write_mib_s,read_mib_s,verify_errors\n");
	for (iteration = 1; iteration <= cfg.iterations; iteration++) {
		if (run_iteration(&cfg, iteration, csv, error, sizeof(error)) != 0) {
			fprintf(stderr, "extentexperiment: %s\n", error);
			fclose(csv);
			return 1;
		}
	}
	if (fclose(csv) != 0) {
		fprintf(stderr, "extentexperiment: failed to close CSV\n");
		return 1;
	}
	printf("CSV results written to %s\n", cfg.csv_output);
	return 0;
}
