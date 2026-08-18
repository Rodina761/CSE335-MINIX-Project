#ifndef EXTENTEXPERIMENT_H
#define EXTENTEXPERIMENT_H

#include <stddef.h>

#define EXTENT_PATH_MAX 512
#define EXTENT_ERROR_MAX 256

struct extent_config {
	unsigned int extent_blocks;
	unsigned int block_size;
	unsigned int file_blocks;
	unsigned int iterations;
	char directory[EXTENT_PATH_MAX];
	char csv_output[EXTENT_PATH_MAX];
};

void extent_config_defaults(struct extent_config *cfg);
int extent_config_load(const char *path, struct extent_config *cfg,
	char *error, size_t error_size);

#endif
