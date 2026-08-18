#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extentexperiment.h"

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

static int parse_uint(const char *text, unsigned int *value)
{
	char *end;
	unsigned long parsed;

	errno = 0;
	parsed = strtoul(text, &end, 0);
	if (errno != 0 || end == text || *trim(end) != '\0' ||
	    parsed == 0 || parsed > 0xffffffffUL)
		return -1;
	*value = (unsigned int)parsed;
	return 0;
}

void extent_config_defaults(struct extent_config *cfg)
{
	memset(cfg, 0, sizeof(*cfg));
	cfg->extent_blocks = 8;
	cfg->block_size = 4096;
	cfg->file_blocks = 512;
	cfg->iterations = 3;
	strcpy(cfg->directory, "/tmp/extentexperiment");
	strcpy(cfg->csv_output, "/tmp/extentexperiment.csv");
}

static int set_value(struct extent_config *cfg, const char *key,
	const char *value, char *error, size_t error_size)
{
	if (strcmp(key, "extent_blocks") == 0)
		return parse_uint(value, &cfg->extent_blocks);
	if (strcmp(key, "block_size") == 0)
		return parse_uint(value, &cfg->block_size);
	if (strcmp(key, "file_blocks") == 0)
		return parse_uint(value, &cfg->file_blocks);
	if (strcmp(key, "iterations") == 0)
		return parse_uint(value, &cfg->iterations);
	if (strcmp(key, "directory") == 0) {
		if (strlen(value) >= sizeof(cfg->directory))
			return -1;
		strcpy(cfg->directory, value);
		return 0;
	}
	if (strcmp(key, "csv_output") == 0) {
		if (strlen(value) >= sizeof(cfg->csv_output))
			return -1;
		strcpy(cfg->csv_output, value);
		return 0;
	}
	snprintf(error, error_size, "unknown configuration key: %s", key);
	return -2;
}

int extent_config_load(const char *path, struct extent_config *cfg,
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
		int status;

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
		status = set_value(cfg, key, value, error, error_size);
		if (status != 0) {
			if (status == -1)
				snprintf(error, error_size,
				    "%s:%lu: invalid value for %s", path,
				    line_number, key);
			fclose(file);
			return -1;
		}
	}
	fclose(file);
	if (cfg->block_size < 512 ||
	    (cfg->block_size & (cfg->block_size - 1)) != 0) {
		snprintf(error, error_size,
		    "block_size must be a power of two of at least 512 bytes");
		return -1;
	}
	if (cfg->extent_blocks > cfg->file_blocks) {
		snprintf(error, error_size,
		    "extent_blocks cannot exceed file_blocks");
		return -1;
	}
	if (cfg->block_size > 1024 * 1024 ||
	    cfg->extent_blocks > (64 * 1024 * 1024U) / cfg->block_size ||
	    cfg->file_blocks > (512 * 1024 * 1024U) / cfg->block_size) {
		snprintf(error, error_size,
		    "buffer or file size exceeds the experiment safety limit");
		return -1;
	}
	return 0;
}
