# Requirement 2 — hierarchical paging and replacement discussion guide

## What the specification asks for

Implement configurable hierarchical paging with user-defined page size,
levels, and address format. Implement FIFO and LRU replacement, collect page
faults, empty frames, and related metrics while page size and hierarchy depth
change, and analyze the results.

## 30-second explanation

`vmexperiment` is compiled and run natively in MINIX. It validates a configurable
virtual-address split, constructs a sparse multi-level page-table structure,
generates a deterministic byte-address trace, and sends the same trace through
FIFO and exact LRU frame replacement. It records hits, faults, replacements,
empty frames, and hierarchy memory in CSV. It is an OS experiment model; it does
not claim to reprogram the fixed x86 MMU.

## Architecture and code map

| File | Responsibility |
|---|---|
| `etc/paging.conf` | Address geometry, frames, algorithms, trace, output |
| `minix/commands/vmexperiment/config.c` | Parsing and geometry validation |
| `minix/commands/vmexperiment/simulator.c` | Sparse hierarchy, trace generation, FIFO/LRU, metrics |
| `minix/commands/vmexperiment/vmexperiment.c` | Runs both policies and writes CSV |
| `minix/commands/vmexperiment/tests/` | Known reference-string oracle |
| `minix/commands/vmexperiment/run_vmexperiments.sh` | 72-row controlled matrix |

Important functions:

- `page_lookup()` extracts level indices and creates sparse paths.
- `select_frame()` chooses an empty frame or a FIFO/LRU victim.
- `access_page()` counts references, hits, faults, and replacements.
- Trace generators create sequential, random, locality, or file-based input.

## Exact code to study

### 1. Configuration geometry gate — `config.c:271-278`

```c
if (hierarchy_bits + offset_bits != cfg->address_bits) {
	snprintf(error, error_size,
	    "sum(level_bits) + log2(page_size) must equal address_bits");
	return -1;
}
if (cfg->frames == 0) {
	snprintf(error, error_size, "frames must be greater than zero");
	return -1;
}
```

Likely question: **Why must the sum be exact?** Every address bit must belong to
exactly one level index or the page offset. Missing or overlapping bits would
make translation ambiguous.

### 2. Extracting a hierarchy index — `simulator.c:212-223`

```c
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
```

The shift discards indices below the requested level; the mask keeps only that
level's configured width.

Likely question: **Why pass a virtual page rather than a byte address?** Page
offset bits are removed first by dividing the byte address by page size.

### 3. Sparse hierarchy creation — `simulator.c:236-262`

```c
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
```

Likely question: **Why call it sparse?** `slot_create()` and `node_create()` are
called only after a referenced index is missing; untouched address regions
consume no lower-level nodes.

### 4. FIFO/LRU victim selection — `simulator.c:274-292`

```c
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
```

Likely question: **What is the only policy-dependent line?** The timestamp:
FIFO compares `loaded_at`; LRU compares `last_access`. Both first use empty
frames before evicting anything.

### 5. Hit, fault, and replacement accounting — `simulator.c:303-325`

```c
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
```

Likely question: **Why is every replacement a fault but not every fault a
replacement?** An initial load into an empty frame faults without evicting a
resident page.

## Address geometry

If address width is `A`, page size is `2^p`, and hierarchy index widths are
`b1 ... bn`, a configuration is valid only when:

```text
b1 + b2 + ... + bn + p = A
```

For the default 32-bit address and 4 KiB page, the offset is 12 bits. Two
10-bit levels consume the remaining 20 bits. The virtual page number is
`address / page_size`; masks and shifts extract indices from it.

The hierarchy is sparse: it allocates only paths touched by the trace. This is
why the experiment can compare table nodes, entries, and approximate memory
instead of allocating every theoretical entry.

## Configuration to know

```ini
address_bits=32
page_size=4096
levels=2
level_bits=10,10
frames=64
algorithm=BOTH
trace_mode=locality
references=10000
working_set_bytes=1048576
hot_bytes=131072
access_stride=64
seed=335
```

The trace uses byte addresses, not page numbers. Keeping the byte working set
fixed is essential when page size changes; otherwise increasing page size would
also silently increase the represented workload.

## FIFO and LRU

FIFO stores `loaded_at`. A hit does not change FIFO order. When frames are full,
the page with the oldest load time is evicted. FIFO is simple but ignores
locality and may show Belady's anomaly.

LRU stores `last_access`. A load and every hit refresh recency. When frames are
full, the least recently accessed page is evicted. Exact LRU is a stack
algorithm, so adding frames cannot increase faults for a fixed trace. Real
kernels often approximate LRU because updating exact order on every access is
expensive.

## Metrics and invariants

- `references`: all address accesses.
- `hits`: requested page already resident.
- `page_faults`: requested page not resident, including first fills.
- `replacements`: faults that evicted a page after frames became full.
- `empty_frames`: unused frames after the trace.
- `hit_ratio`: hits divided by references.
- hierarchy nodes, entries, and bytes: sparse metadata cost.

The key invariant is:

```text
references = hits + page_faults
```

First fills are faults but not replacements. Therefore replacements cannot be
greater than faults.

## Native results

For the controlled locality trace with 64 frames:

| Page size | FIFO faults | LRU faults |
|---|---:|---:|
| 1,024 B | 6,937 | 6,791 |
| 2,048 B | 4,735 | 4,047 |
| 4,096 B | 2,666 | 1,655 |
| 8,192 B | 1,378 | 1,064 |

LRU performed better on this locality-heavy workload because hot pages refresh
their recency. Faults decreased as page size increased because every frame
covered more of the same fixed byte working set. Larger pages are not always
better: they may increase internal fragmentation and transfer unused data.

Changing levels primarily changes hierarchy depth and metadata, not policy
faults, when the page size, frames, and trace are identical.

## Known-answer test

The trace is `1 2 3 4 1 2 5 1 2 3 4 5` with three frames. Expected faults:

```text
FIFO = 9
LRU  = 10
```

This is a useful discussion point: LRU is not guaranteed to beat FIFO on every
individual reference string. Its advantage appears for workloads with useful
temporal locality.

## Demo

```sh
sed -n '1,120p' /etc/paging.conf
vmexperiment -c /etc/paging.conf
head -5 /tmp/vmexperiment.csv

cd /usr/src/minix/commands/vmexperiment/tests
sh test_known.sh
```

Expected final line: `PASS: known FIFO/LRU reference string`.

For a quick configuration demonstration, change `frames`, run the same seeded
trace, and compare faults. Do not change multiple variables at once if you want
to explain causation.

## Likely questions and model answers

### Is this the hardware page table used by the CPU?

No. It is a configurable hierarchy and replacement experiment running inside
MINIX. The x86 hardware page format is fixed and cannot be changed arbitrarily
from a text file. The final CSV leaves unreliable real VM telemetry fields as
`NA` rather than mislabelling simulator results as hardware faults.

### Why hierarchical paging?

A flat table needs space for all virtual pages. A hierarchy allocates lower
levels only for used address regions, trading extra lookups for potentially much
lower metadata memory on sparse address spaces.

### Does increasing levels reduce faults?

Not by itself. Replacement faults depend on the page-reference sequence, page
size, frames, and policy. More levels primarily change translation structure
and metadata overhead.

### Why do larger pages reduce faults in your results?

The trace represents a fixed number of bytes. A larger page brings more of that
working set into each frame, so fewer distinct pages are needed. The trade-off
is possible fragmentation and unnecessary data transfer.

### Why can LRU have 10 faults when FIFO has 9 in the known test?

Policy quality depends on the trace. The known string happens to favor FIFO.
LRU's stack property prevents Belady's anomaly but does not mean it wins against
every different algorithm on every trace.

### What is Belady's anomaly?

For FIFO, increasing the number of frames can sometimes increase faults. Exact
LRU cannot exhibit this for a fixed reference string because its resident sets
have the stack property.

### Why use a fixed seed?

It produces identical traces across policies and configurations, so observed
differences come from the intended independent variable rather than random
input changes.

## Claims to avoid

- Do not say the experiment changed the CPU's hardware page size.
- Do not call simulator faults real process hardware faults.
- Do not say more hierarchy levels automatically reduce page faults.
- Do not say LRU always produces fewer faults than FIFO.
