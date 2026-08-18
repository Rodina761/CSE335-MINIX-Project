#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __minix
#include <minix/type.h>
#include <minix/vm.h>
#endif

#include "vmexperiment.h"

struct real_vm_stats {
	int available;
	unsigned int page_size;
	unsigned long total;
	unsigned long free;
	unsigned long largest;
	unsigned long cached;
};

static void usage(const char *program)
{
	fprintf(stderr,
	    "Usage: %s [-c config] [-o csv-output] [-t trace-file]\n",
	    program);
}

static void collect_real_stats(struct real_vm_stats *stats)
{
	memset(stats, 0, sizeof(*stats));
#ifdef __minix
	{
		struct vm_stats_info info;

		/* Some MINIX 3.3 VM images do not reply to VM_INFO for an
		 * unprivileged command. Keep native counters opt-in so the
		 * deterministic experiment cannot block indefinitely. */
		if (getenv("VMEXP_REAL_STATS") == NULL)
			return;
		if (vm_info_stats(&info) == 0) {
			stats->available = 1;
			stats->page_size = info.vsi_pagesize;
			stats->total = info.vsi_total;
			stats->free = info.vsi_free;
			stats->largest = info.vsi_largest;
			stats->cached = info.vsi_cached;
		}
	}
#endif
}

static void print_configuration(const struct vmexp_config *cfg,
	const struct real_vm_stats *before)
{
	unsigned int level;

	printf("VM experiment configuration\n");
	printf("  address bits: %u\n", cfg->address_bits);
	printf("  simulated page size: %llu bytes\n",
	    (unsigned long long)cfg->page_size);
	printf("  hierarchy: %u levels (", cfg->levels);
	for (level = 0; level < cfg->levels; level++)
		printf("%s%u", level == 0 ? "" : ",", cfg->level_bits[level]);
	printf(" bits)\n");
	printf("  simulated frames: %u\n", cfg->frames);
	printf("  replacement: %s\n", policy_name(cfg->policy));
	printf("  trace: %s\n", trace_mode_name(cfg->trace_mode));
	if (before->available) {
		printf("Actual MINIX VM before run\n");
		printf("  hardware page size: %u bytes\n", before->page_size);
		printf("  total/free/cached frames: %lu/%lu/%lu\n",
		    before->total, before->free, before->cached);
	}
}

static void print_result(const struct simulation_result *result)
{
	double hit_ratio;

	hit_ratio = result->references == 0 ? 0.0 :
	    (double)result->hits / (double)result->references;
	printf("%s results\n", policy_name(result->policy));
	printf("  references: %llu\n",
	    (unsigned long long)result->references);
	printf("  hits: %llu\n", (unsigned long long)result->hits);
	printf("  page faults: %llu\n",
	    (unsigned long long)result->page_faults);
	printf("  replacements: %llu\n",
	    (unsigned long long)result->replacements);
	printf("  empty frames: %llu\n",
	    (unsigned long long)result->empty_frames);
	printf("  hit ratio: %.6f\n", hit_ratio);
	printf("  hierarchy nodes/entries/bytes: %llu/%llu/%llu\n",
	    (unsigned long long)result->page_table_nodes,
	    (unsigned long long)result->page_table_entries,
	    (unsigned long long)result->page_table_bytes);
}

static int write_csv(const char *path, const struct vmexp_config *cfg,
	const struct simulation_result *results, size_t result_count,
	const struct real_vm_stats *before, const struct real_vm_stats *after)
{
	FILE *file;
	size_t index;

	file = fopen(path, "w");
	if (file == NULL) {
		fprintf(stderr, "vmexperiment: cannot write %s: %s\n",
		    path, strerror(errno));
		return -1;
	}
	fprintf(file, "algorithm,trace_mode,address_bits,page_size,levels,");
	fprintf(file, "frames,references,hits,page_faults,replacements,");
	fprintf(file, "empty_frames,hit_ratio,page_table_nodes,");
	fprintf(file, "page_table_entries,page_table_bytes,");
	fprintf(file, "real_page_size,real_free_before,real_free_after,");
	fprintf(file, "real_cached_before,real_cached_after\n");
	for (index = 0; index < result_count; index++) {
		const struct simulation_result *result;
		double hit_ratio;

		result = &results[index];
		hit_ratio = result->references == 0 ? 0.0 :
		    (double)result->hits / (double)result->references;
		fprintf(file, "%s,%s,%u,%llu,%u,%u,%llu,%llu,%llu,%llu,",
		    policy_name(result->policy), trace_mode_name(cfg->trace_mode),
		    cfg->address_bits, (unsigned long long)cfg->page_size,
		    cfg->levels, cfg->frames,
		    (unsigned long long)result->references,
		    (unsigned long long)result->hits,
		    (unsigned long long)result->page_faults,
		    (unsigned long long)result->replacements);
		fprintf(file, "%llu,%.6f,%llu,%llu,%llu,",
		    (unsigned long long)result->empty_frames, hit_ratio,
		    (unsigned long long)result->page_table_nodes,
		    (unsigned long long)result->page_table_entries,
		    (unsigned long long)result->page_table_bytes);
		if (before->available && after->available)
			fprintf(file, "%u,%lu,%lu,%lu,%lu\n", before->page_size,
			    before->free, after->free, before->cached, after->cached);
		else
			fprintf(file, "NA,NA,NA,NA,NA\n");
	}
	if (fclose(file) != 0) {
		fprintf(stderr, "vmexperiment: error closing %s\n", path);
		return -1;
	}
	return 0;
}

int main(int argc, char **argv)
{
	const char *config_path;
	const char *output_override;
	const char *trace_override;
	struct vmexp_config cfg;
	struct simulation_result results[2];
	struct real_vm_stats before;
	struct real_vm_stats after;
	uint64_t *addresses;
	size_t address_count;
	size_t result_count;
	char error[VMEXP_ERROR_MAX];
	int index;

	config_path = "/etc/paging.conf";
	output_override = NULL;
	trace_override = NULL;
	for (index = 1; index < argc; index++) {
		if (strcmp(argv[index], "-c") == 0 && index + 1 < argc)
			config_path = argv[++index];
		else if (strcmp(argv[index], "-o") == 0 && index + 1 < argc)
			output_override = argv[++index];
		else if (strcmp(argv[index], "-t") == 0 && index + 1 < argc)
			trace_override = argv[++index];
		else {
			usage(argv[0]);
			return 2;
		}
	}
	config_defaults(&cfg);
	fprintf(stderr, "vmexperiment: loading configuration\n");
	if (config_load(config_path, &cfg, error, sizeof(error)) != 0) {
		fprintf(stderr, "vmexperiment: %s\n", error);
		return 1;
	}
	if (output_override != NULL) {
		if (strlen(output_override) >= sizeof(cfg.csv_output)) {
			fprintf(stderr, "vmexperiment: output path is too long\n");
			return 1;
		}
		strcpy(cfg.csv_output, output_override);
	}
	if (trace_override != NULL) {
		if (strlen(trace_override) >= sizeof(cfg.trace_file)) {
			fprintf(stderr, "vmexperiment: trace path is too long\n");
			return 1;
		}
		strcpy(cfg.trace_file, trace_override);
		cfg.trace_mode = TRACE_FILE;
	}
	if (config_validate(&cfg, error, sizeof(error)) != 0) {
		fprintf(stderr, "vmexperiment: %s\n", error);
		return 1;
	}
	fprintf(stderr, "vmexperiment: building address trace\n");
	if (trace_build(&cfg, &addresses, &address_count,
	    error, sizeof(error)) != 0) {
		fprintf(stderr, "vmexperiment: %s\n", error);
		return 1;
	}
	fprintf(stderr, "vmexperiment: collecting optional VM context\n");
	collect_real_stats(&before);
	fprintf(stderr, "vmexperiment: starting replacement simulation\n");
	print_configuration(&cfg, &before);
	result_count = 0;
	if (cfg.policy == POLICY_FIFO || cfg.policy == POLICY_BOTH) {
		fprintf(stderr, "vmexperiment: simulating FIFO\n");
		if (simulate(&cfg, addresses, address_count, POLICY_FIFO,
		    &results[result_count], error, sizeof(error)) != 0) {
			fprintf(stderr, "vmexperiment: %s\n", error);
			free(addresses);
			return 1;
		}
		print_result(&results[result_count++]);
	}
	if (cfg.policy == POLICY_LRU || cfg.policy == POLICY_BOTH) {
		fprintf(stderr, "vmexperiment: simulating LRU\n");
		if (simulate(&cfg, addresses, address_count, POLICY_LRU,
		    &results[result_count], error, sizeof(error)) != 0) {
			fprintf(stderr, "vmexperiment: %s\n", error);
			free(addresses);
			return 1;
		}
		print_result(&results[result_count++]);
	}
	fprintf(stderr, "vmexperiment: writing CSV\n");
	collect_real_stats(&after);
	if (write_csv(cfg.csv_output, &cfg, results, result_count,
	    &before, &after) != 0) {
		free(addresses);
		return 1;
	}
	printf("CSV results written to %s\n", cfg.csv_output);
	free(addresses);
	fprintf(stderr, "vmexperiment: complete\n");
	/* MINIX 3.3's legacy stdio cleanup may block after VM-related work.
	 * All durable output (the CSV) has already been closed by write_csv(). */
	_exit(0);
}
