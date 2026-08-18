#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vmexperiment.h"

struct page_entry {
	uint64_t virtual_page;
	uint64_t loaded_at;
	uint64_t last_access;
	unsigned int frame;
	int present;
};

struct table_slot {
	uint64_t index;
	void *value;
	struct table_slot *next;
};

struct table_node {
	unsigned int level;
	struct table_slot *slots;
};

struct simulator {
	const struct vmexp_config *cfg;
	struct table_node *root;
	struct page_entry **frames;
	uint64_t tick;
	struct simulation_result result;
};

static uint32_t random_next(uint32_t *state)
{
	*state = *state * 1664525U + 1013904223U;
	return *state;
}

static int append_address(uint64_t **items, size_t *count, size_t *capacity,
	uint64_t address, char *error, size_t error_size)
{
	uint64_t *grown;
	size_t new_capacity;

	if (*count == *capacity) {
		new_capacity = *capacity == 0 ? 1024 : *capacity * 2;
		if (new_capacity < *capacity ||
		    new_capacity > ((size_t)-1) / sizeof(**items)) {
			snprintf(error, error_size, "trace is too large");
			return -1;
		}
		grown = realloc(*items, new_capacity * sizeof(**items));
		if (grown == NULL) {
			snprintf(error, error_size, "not enough memory for trace");
			return -1;
		}
		*items = grown;
		*capacity = new_capacity;
	}
	(*items)[(*count)++] = address;
	return 0;
}

static int trace_from_file(const struct vmexp_config *cfg,
	uint64_t **addresses, size_t *count, char *error, size_t error_size)
{
	FILE *file;
	char line[512];
	size_t capacity;
	unsigned long line_number;

	file = fopen(cfg->trace_file, "r");
	if (file == NULL) {
		snprintf(error, error_size, "cannot open trace %s: %s",
		    cfg->trace_file, strerror(errno));
		return -1;
	}
	capacity = 0;
	line_number = 0;
	while (fgets(line, sizeof(line), file) != NULL) {
		char *text;
		char *end;
		char *comment;
		uint64_t address;

		line_number++;
		comment = strchr(line, '#');
		if (comment != NULL)
			*comment = '\0';
		text = line;
		while (*text == ' ' || *text == '\t' || *text == '\r' ||
		    *text == '\n')
			text++;
		if (*text == '\0')
			continue;
		errno = 0;
		address = strtoull(text, &end, 0);
		while (*end == ' ' || *end == '\t' || *end == '\r' ||
		    *end == '\n')
			end++;
		if (errno != 0 || end == text || *end != '\0') {
			snprintf(error, error_size,
			    "%s:%lu: invalid address", cfg->trace_file, line_number);
			fclose(file);
			free(*addresses);
			*addresses = NULL;
			*count = 0;
			return -1;
		}
		if (append_address(addresses, count, &capacity, address,
		    error, error_size) != 0) {
			fclose(file);
			free(*addresses);
			*addresses = NULL;
			*count = 0;
			return -1;
		}
	}
	fclose(file);
	if (*count == 0) {
		snprintf(error, error_size, "trace file contains no addresses");
		free(*addresses);
		*addresses = NULL;
		return -1;
	}
	return 0;
}

int trace_build(const struct vmexp_config *cfg, uint64_t **addresses,
	size_t *count, char *error, size_t error_size)
{
	uint64_t index;
	uint32_t state;

	*addresses = NULL;
	*count = 0;
	if (cfg->trace_mode == TRACE_FILE)
		return trace_from_file(cfg, addresses, count, error, error_size);
	if (cfg->references > ((size_t)-1) / sizeof(**addresses)) {
		snprintf(error, error_size, "reference count is too large");
		return -1;
	}
	*addresses = malloc((size_t)cfg->references * sizeof(**addresses));
	if (*addresses == NULL) {
		snprintf(error, error_size, "not enough memory for trace");
		return -1;
	}
	state = cfg->seed;
	for (index = 0; index < cfg->references; index++) {
		if (cfg->trace_mode == TRACE_SEQUENTIAL) {
			(*addresses)[index] =
			    (index * cfg->access_stride) % cfg->working_set_bytes;
		} else if (cfg->trace_mode == TRACE_RANDOM) {
			(*addresses)[index] =
			    random_next(&state) % cfg->working_set_bytes;
		} else {
			if ((random_next(&state) % 100) < 80)
				(*addresses)[index] =
				    random_next(&state) % cfg->hot_bytes;
			else
				(*addresses)[index] =
				    random_next(&state) % cfg->working_set_bytes;
		}
	}
	*count = (size_t)cfg->references;
	return 0;
}

static struct table_node *node_create(struct simulator *sim,
	unsigned int level)
{
	struct table_node *node;

	node = calloc(1, sizeof(*node));
	if (node != NULL) {
		node->level = level;
		sim->result.page_table_nodes++;
		sim->result.page_table_bytes += sizeof(*node);
	}
	return node;
}

static struct table_slot *slot_find(struct table_node *node, uint64_t index)
{
	struct table_slot *slot;

	for (slot = node->slots; slot != NULL; slot = slot->next) {
		if (slot->index == index)
			return slot;
	}
	return NULL;
}

static struct table_slot *slot_create(struct simulator *sim,
	struct table_node *node, uint64_t index)
{
	struct table_slot *slot;

	slot = calloc(1, sizeof(*slot));
	if (slot == NULL)
		return NULL;
	slot->index = index;
	slot->next = node->slots;
	node->slots = slot;
	sim->result.page_table_entries++;
	sim->result.page_table_bytes += sizeof(*slot);
	return slot;
}

static uint64_t level_index(const struct vmexp_config *cfg,
	uint64_t virtual_page, unsigned int wanted_level)
{
	unsigned int level;
	unsigned int lower_bits;
	uint64_t mask;

	lower_bits = 0;
	for (level = wanted_level + 1; level < cfg->levels; level++)
		lower_bits += cfg->level_bits[level];
	mask = ((uint64_t)1 << cfg->level_bits[wanted_level]) - 1;
	return (virtual_page >> lower_bits) & mask;
}

static struct page_entry *page_lookup(struct simulator *sim,
	uint64_t virtual_page, int create)
{
	struct table_node *node;
	struct table_node *child;
	struct table_slot *slot;
	struct page_entry *page;
	unsigned int level;
	uint64_t index;

	node = sim->root;
	for (level = 0; level < sim->cfg->levels; level++) {
		index = level_index(sim->cfg, virtual_page, level);
		slot = slot_find(node, index);
		if (slot == NULL) {
			if (!create)
				return NULL;
			slot = slot_create(sim, node, index);
			if (slot == NULL)
				return NULL;
			if (level + 1 == sim->cfg->levels) {
				page = calloc(1, sizeof(*page));
				if (page == NULL)
					return NULL;
				page->virtual_page = virtual_page;
				slot->value = page;
				sim->result.page_table_bytes += sizeof(*page);
			} else {
				child = node_create(sim, level + 1);
				if (child == NULL)
					return NULL;
				slot->value = child;
			}
		}
		if (level + 1 == sim->cfg->levels)
			return (struct page_entry *)slot->value;
		node = (struct table_node *)slot->value;
	}
	return NULL;
}

static unsigned int select_frame(struct simulator *sim,
	enum replacement_policy policy)
{
	unsigned int frame;
	unsigned int selected;
	uint64_t selected_time;

	for (frame = 0; frame < sim->cfg->frames; frame++) {
		if (sim->frames[frame] == NULL)
			return frame;
	}
	selected = 0;
	selected_time = policy == POLICY_FIFO ?
	    sim->frames[0]->loaded_at : sim->frames[0]->last_access;
	for (frame = 1; frame < sim->cfg->frames; frame++) {
		uint64_t candidate_time;

		candidate_time = policy == POLICY_FIFO ?
		    sim->frames[frame]->loaded_at :
		    sim->frames[frame]->last_access;
		if (candidate_time < selected_time) {
			selected = frame;
			selected_time = candidate_time;
		}
	}
	return selected;
}

static int access_page(struct simulator *sim, uint64_t address,
	enum replacement_policy policy)
{
	struct page_entry *page;
	struct page_entry *victim;
	uint64_t virtual_page;
	unsigned int frame;

	virtual_page = address / sim->cfg->page_size;
	page = page_lookup(sim, virtual_page, 1);
	if (page == NULL)
		return -1;
	sim->tick++;
	sim->result.references++;
	if (page->present) {
		sim->result.hits++;
		page->last_access = sim->tick;
		return 0;
	}
	sim->result.page_faults++;
	frame = select_frame(sim, policy);
	victim = sim->frames[frame];
	if (victim != NULL) {
		victim->present = 0;
		sim->result.replacements++;
	}
	page->present = 1;
	page->frame = frame;
	page->loaded_at = sim->tick;
	page->last_access = sim->tick;
	sim->frames[frame] = page;
	return 0;
}

static void node_destroy(struct table_node *node, unsigned int levels)
{
	struct table_slot *slot;
	struct table_slot *next;

	if (node == NULL)
		return;
	for (slot = node->slots; slot != NULL; slot = next) {
		next = slot->next;
		if (node->level + 1 == levels)
			free(slot->value);
		else
			node_destroy((struct table_node *)slot->value, levels);
		free(slot);
	}
	free(node);
}

int simulate(const struct vmexp_config *cfg, const uint64_t *addresses,
	size_t count, enum replacement_policy policy,
	struct simulation_result *result, char *error, size_t error_size)
{
	struct simulator sim;
	size_t index;
	unsigned int frame;

	memset(&sim, 0, sizeof(sim));
	sim.cfg = cfg;
	sim.result.policy = policy;
	sim.root = node_create(&sim, 0);
	sim.frames = calloc(cfg->frames, sizeof(*sim.frames));
	if (sim.root == NULL || sim.frames == NULL) {
		snprintf(error, error_size, "not enough memory for simulator");
		node_destroy(sim.root, cfg->levels);
		free(sim.frames);
		return -1;
	}
	for (index = 0; index < count; index++) {
		if (access_page(&sim, addresses[index], policy) != 0) {
			snprintf(error, error_size,
			    "not enough memory while translating reference %lu",
			    (unsigned long)index);
			node_destroy(sim.root, cfg->levels);
			free(sim.frames);
			return -1;
		}
	}
	for (frame = 0; frame < cfg->frames; frame++) {
		if (sim.frames[frame] == NULL)
			sim.result.empty_frames++;
	}
	*result = sim.result;
	node_destroy(sim.root, cfg->levels);
	free(sim.frames);
	return 0;
}
