#ifndef VMEXPERIMENT_H
#define VMEXPERIMENT_H

#include <stddef.h>
#include <stdint.h>

#define VMEXP_MAX_LEVELS 8
#define VMEXP_PATH_MAX 512
#define VMEXP_ERROR_MAX 256

enum replacement_policy {
	POLICY_FIFO = 0,
	POLICY_LRU = 1,
	POLICY_BOTH = 2
};

enum trace_mode {
	TRACE_SEQUENTIAL = 0,
	TRACE_LOCALITY = 1,
	TRACE_RANDOM = 2,
	TRACE_FILE = 3
};

struct vmexp_config {
	unsigned int address_bits;
	uint64_t page_size;
	unsigned int levels;
	unsigned int level_bits[VMEXP_MAX_LEVELS];
	unsigned int level_bits_count;
	unsigned int frames;
	enum replacement_policy policy;
	enum trace_mode trace_mode;
	uint64_t references;
	uint64_t working_set_bytes;
	uint64_t hot_bytes;
	uint64_t access_stride;
	uint32_t seed;
	char trace_file[VMEXP_PATH_MAX];
	char csv_output[VMEXP_PATH_MAX];
};

struct simulation_result {
	enum replacement_policy policy;
	uint64_t references;
	uint64_t hits;
	uint64_t page_faults;
	uint64_t replacements;
	uint64_t empty_frames;
	uint64_t page_table_nodes;
	uint64_t page_table_entries;
	uint64_t page_table_bytes;
};

void config_defaults(struct vmexp_config *cfg);
int config_load(const char *path, struct vmexp_config *cfg,
	char *error, size_t error_size);
int config_validate(const struct vmexp_config *cfg,
	char *error, size_t error_size);
const char *policy_name(enum replacement_policy policy);
const char *trace_mode_name(enum trace_mode mode);

int trace_build(const struct vmexp_config *cfg, uint64_t **addresses,
	size_t *count, char *error, size_t error_size);
int simulate(const struct vmexp_config *cfg, const uint64_t *addresses,
	size_t count, enum replacement_policy policy,
	struct simulation_result *result, char *error, size_t error_size);

#endif
