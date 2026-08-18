#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vmexperiment.h"

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

static int equals_ignore_case(const char *left, const char *right)
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

static int parse_u64(const char *text, uint64_t *value)
{
	char *end;
	unsigned long long parsed;

	errno = 0;
	parsed = strtoull(text, &end, 0);
	if (errno != 0 || end == text || *trim(end) != '\0')
		return -1;
	*value = (uint64_t)parsed;
	return 0;
}

static int parse_level_bits(const char *text, struct vmexp_config *cfg)
{
	char buffer[256];
	char *token;
	unsigned int count;
	uint64_t value;

	if (strlen(text) >= sizeof(buffer))
		return -1;
	strcpy(buffer, text);
	count = 0;
	for (token = strtok(buffer, ","); token != NULL;
	    token = strtok(NULL, ",")) {
		if (count >= VMEXP_MAX_LEVELS ||
		    parse_u64(trim(token), &value) != 0 || value == 0 ||
		    value > 31)
			return -1;
		cfg->level_bits[count++] = (unsigned int)value;
	}
	cfg->level_bits_count = count;
	return count > 0 ? 0 : -1;
}

void config_defaults(struct vmexp_config *cfg)
{
	memset(cfg, 0, sizeof(*cfg));
	cfg->address_bits = 32;
	cfg->page_size = 4096;
	cfg->levels = 2;
	cfg->level_bits[0] = 10;
	cfg->level_bits[1] = 10;
	cfg->level_bits_count = 2;
	cfg->frames = 64;
	cfg->policy = POLICY_BOTH;
	cfg->trace_mode = TRACE_LOCALITY;
	cfg->references = 10000;
	cfg->working_set_bytes = 1024 * 1024;
	cfg->hot_bytes = 128 * 1024;
	cfg->access_stride = 64;
	cfg->seed = 335;
	strcpy(cfg->csv_output, "/tmp/vmexperiment.csv");
}

static int set_value(struct vmexp_config *cfg, const char *key,
	const char *value, char *error, size_t error_size)
{
	uint64_t number;

	if (equals_ignore_case(key, "algorithm")) {
		if (equals_ignore_case(value, "FIFO"))
			cfg->policy = POLICY_FIFO;
		else if (equals_ignore_case(value, "LRU"))
			cfg->policy = POLICY_LRU;
		else if (equals_ignore_case(value, "BOTH"))
			cfg->policy = POLICY_BOTH;
		else
			goto invalid;
		return 0;
	}
	if (equals_ignore_case(key, "trace_mode")) {
		if (equals_ignore_case(value, "sequential"))
			cfg->trace_mode = TRACE_SEQUENTIAL;
		else if (equals_ignore_case(value, "locality"))
			cfg->trace_mode = TRACE_LOCALITY;
		else if (equals_ignore_case(value, "random"))
			cfg->trace_mode = TRACE_RANDOM;
		else if (equals_ignore_case(value, "file"))
			cfg->trace_mode = TRACE_FILE;
		else
			goto invalid;
		return 0;
	}
	if (equals_ignore_case(key, "trace_file")) {
		if (strlen(value) >= sizeof(cfg->trace_file))
			goto invalid;
		strcpy(cfg->trace_file, value);
		return 0;
	}
	if (equals_ignore_case(key, "csv_output")) {
		if (strlen(value) >= sizeof(cfg->csv_output))
			goto invalid;
		strcpy(cfg->csv_output, value);
		return 0;
	}
	if (equals_ignore_case(key, "level_bits")) {
		if (parse_level_bits(value, cfg) != 0)
			goto invalid;
		return 0;
	}
	if (parse_u64(value, &number) != 0)
		goto invalid;
	if (equals_ignore_case(key, "address_bits"))
		cfg->address_bits = (unsigned int)number;
	else if (equals_ignore_case(key, "page_size"))
		cfg->page_size = number;
	else if (equals_ignore_case(key, "levels"))
		cfg->levels = (unsigned int)number;
	else if (equals_ignore_case(key, "frames"))
		cfg->frames = (unsigned int)number;
	else if (equals_ignore_case(key, "references"))
		cfg->references = number;
	else if (equals_ignore_case(key, "working_set_bytes"))
		cfg->working_set_bytes = number;
	else if (equals_ignore_case(key, "hot_bytes"))
		cfg->hot_bytes = number;
	else if (equals_ignore_case(key, "access_stride"))
		cfg->access_stride = number;
	else if (equals_ignore_case(key, "seed"))
		cfg->seed = (uint32_t)number;
	else {
		snprintf(error, error_size, "unknown configuration key: %s", key);
		return -1;
	}
	return 0;

invalid:
	snprintf(error, error_size, "invalid value for %s: %s", key, value);
	return -1;
}

int config_load(const char *path, struct vmexp_config *cfg,
	char *error, size_t error_size)
{
	FILE *file;
	char line[1024];
	unsigned long line_number;

	file = fopen(path, "r");
	if (file == NULL) {
		snprintf(error, error_size, "cannot open configuration %s: %s",
		    path, strerror(errno));
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
			snprintf(error, error_size,
			    "%s:%lu: expected key=value", path, line_number);
			fclose(file);
			return -1;
		}
		*separator = '\0';
		value = trim(separator + 1);
		key = trim(key);
		if (set_value(cfg, key, value, error, error_size) != 0) {
			char detail[VMEXP_ERROR_MAX];
			strncpy(detail, error, sizeof(detail) - 1);
			detail[sizeof(detail) - 1] = '\0';
			snprintf(error, error_size, "%s:%lu: %s", path,
			    line_number, detail);
			fclose(file);
			return -1;
		}
	}
	if (ferror(file)) {
		snprintf(error, error_size, "error reading %s", path);
		fclose(file);
		return -1;
	}
	fclose(file);
	return config_validate(cfg, error, error_size);
}

static unsigned int power_of_two_shift(uint64_t value)
{
	unsigned int shift;

	if (value == 0 || (value & (value - 1)) != 0)
		return 0;
	shift = 0;
	while (value > 1) {
		value >>= 1;
		shift++;
	}
	return shift;
}

int config_validate(const struct vmexp_config *cfg,
	char *error, size_t error_size)
{
	unsigned int offset_bits;
	unsigned int hierarchy_bits;
	unsigned int level;

	if (cfg->address_bits < 8 || cfg->address_bits > 63) {
		snprintf(error, error_size, "address_bits must be between 8 and 63");
		return -1;
	}
	offset_bits = power_of_two_shift(cfg->page_size);
	if (offset_bits == 0 || offset_bits >= cfg->address_bits) {
		snprintf(error, error_size,
		    "page_size must be a power of two smaller than the address space");
		return -1;
	}
	if (cfg->levels == 0 || cfg->levels > VMEXP_MAX_LEVELS) {
		snprintf(error, error_size, "levels must be between 1 and %d",
		    VMEXP_MAX_LEVELS);
		return -1;
	}
	if (cfg->level_bits_count != cfg->levels) {
		snprintf(error, error_size,
		    "level_bits must contain exactly one value per level");
		return -1;
	}
	hierarchy_bits = 0;
	for (level = 0; level < cfg->levels; level++) {
		if (cfg->level_bits[level] == 0 || cfg->level_bits[level] > 31) {
			snprintf(error, error_size, "each level_bits value must be 1..31");
			return -1;
		}
		hierarchy_bits += cfg->level_bits[level];
	}
	if (hierarchy_bits + offset_bits != cfg->address_bits) {
		snprintf(error, error_size,
		    "sum(level_bits) + log2(page_size) must equal address_bits");
		return -1;
	}
	if (cfg->frames == 0) {
		snprintf(error, error_size, "frames must be greater than zero");
		return -1;
	}
	if (cfg->trace_mode == TRACE_FILE) {
		if (cfg->trace_file[0] == '\0') {
			snprintf(error, error_size,
			    "trace_file is required when trace_mode=file");
			return -1;
		}
	} else {
		if (cfg->references == 0 || cfg->working_set_bytes == 0 ||
		    cfg->access_stride == 0) {
			snprintf(error, error_size,
			    "references, working_set_bytes, and access_stride must be greater than zero");
			return -1;
		}
		if (cfg->hot_bytes == 0 ||
		    cfg->hot_bytes > cfg->working_set_bytes) {
			snprintf(error, error_size,
			    "hot_bytes must be between 1 and working_set_bytes");
			return -1;
		}
		if (cfg->working_set_bytes - 1 >
		    (((uint64_t)1 << cfg->address_bits) - 1)) {
			snprintf(error, error_size,
			    "working_set_bytes exceeds the configured address space");
			return -1;
		}
	}
	return 0;
}

const char *policy_name(enum replacement_policy policy)
{
	if (policy == POLICY_FIFO)
		return "FIFO";
	if (policy == POLICY_LRU)
		return "LRU";
	return "BOTH";
}

const char *trace_mode_name(enum trace_mode mode)
{
	if (mode == TRACE_SEQUENTIAL)
		return "sequential";
	if (mode == TRACE_LOCALITY)
		return "locality";
	if (mode == TRACE_RANDOM)
		return "random";
	return "file";
}
