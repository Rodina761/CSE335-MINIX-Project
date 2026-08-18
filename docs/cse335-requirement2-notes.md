# Requirement 2: Configurable Hierarchical Paging and Replacement

## Scope and interpretation

MINIX 3.3.0 for 32-bit x86 already uses hierarchical paging. The architecture
header `minix/include/arch/i386/include/vm.h` defines a 4 KiB page, 1,024 page
directory entries, and 1,024 page-table entries. The VM server maps these
architecture constants through
`minix/servers/vm/arch/i386/pagetable.h`. Consequently, the real hardware
address split is 10 directory bits, 10 table bits, and 12 offset bits. These
parameters describe the processor's MMU interface and cannot be made arbitrary
at run time by reading a text file.

The implementation uses a technically honest experimental interpretation of
the requirement. A new program, `vmexperiment`, is compiled as part of MINIX.
It models user-configurable hierarchical paging and FIFO/LRU replacement over
repeatable address traces. When run on MINIX, it also calls the existing
`vm_info_stats()` interface to record the real system page size and physical
frame statistics before and after each experiment. The output distinguishes
simulated-policy measurements from real MINIX measurements.

This design avoids claiming that a software experiment changed the x86 MMU.
It also avoids an unsafe incomplete replacement of the production VM server.
Stock MINIX 3.3.0 accounts for major and minor faults in
`minix/servers/vm/pagefaults.c` and evicts unreferenced filesystem cache pages
through an LRU list in `minix/servers/vm/cache.c`. That cache list is not a
general anonymous-memory swap subsystem. True FIFO/LRU replacement of all
process pages would additionally require a swap backing store, dirty-page
writeback, page-table invalidation, TLB coordination, and reload-on-fault.

## Hierarchical address translation

For an address width of `A` bits, page size `P = 2^p`, and `N` hierarchy
levels with index widths `b1 ... bN`, a valid configuration satisfies:

`b1 + b2 + ... + bN + p = A`.

The virtual page number is `floor(address / P)`. The simulator extracts each
level index from the virtual page number, beginning with the most significant
configured group of index bits. A sparse linked representation allocates only
nodes and entries reached by the workload. This makes configurations such as a
single 20-bit level practical without allocating an array of over one million
pointers at startup.

The implementation counts allocated hierarchy nodes, allocated entries, and
the approximate bytes occupied by nodes, slots, and leaf page descriptors.
Increasing the number of levels does not inherently change the page-reference
sequence or replacement decisions. It normally changes hierarchy depth and
metadata overhead. This distinction is important when interpreting results.

## FIFO replacement

FIFO records the time at which every resident page entered its frame. On a
fault with no empty frame, it evicts the page with the smallest load time.
Accessing a resident page does not change its FIFO position. FIFO is simple and
has low bookkeeping cost, but it does not exploit temporal locality and can
exhibit Belady's anomaly.

The implementation stores a monotonically increasing `loaded_at` value in each
page descriptor. Empty frames are used before any page is replaced. On
replacement, the victim descriptor is marked nonresident and the requested
page is assigned to that frame.

## LRU replacement

LRU approximates the principle of temporal locality: a page used recently is
likely to be used again soon. Every hit and every page load updates the page's
`last_access` timestamp. On a fault with no empty frame, the simulator evicts
the resident page with the smallest last-access time.

Exact LRU has greater bookkeeping cost than FIFO. Real systems often
approximate LRU with accessed bits, aging counters, or clock algorithms. Exact
timestamps are appropriate here because the goal is to compare algorithmic
behavior rather than minimize simulator overhead.

## Configuration and repeatability

`/etc/paging.conf` controls the address width, page size, number of levels,
bits per level, frame count, algorithm, trace type, trace length, workload byte
range, hot byte range, sequential stride, random seed, input trace, and CSV
destination. The parser rejects unknown keys and inconsistent address formats.

Generated traces are deterministic. Sequential mode walks a fixed byte range
with a configurable stride. Random mode selects addresses from the complete
working-set byte range. Locality mode selects 80 percent of references from a
smaller hot range and 20 percent from the complete range. File mode accepts
one decimal or hexadecimal byte address per line.

FIFO and LRU always receive the same in-memory address trace during a run. The
experiment matrix keeps the address workload, reference count, and seed fixed
while page size, hierarchy depth, and frame count change. Holding bytes rather
than page count constant is essential: holding page count constant would
silently enlarge the workload when page size increased and would make the
comparison misleading.

## Metrics

For each policy the program records:

- References: total virtual-address accesses.
- Hits: references whose page was already resident.
- Page faults: references whose page was not resident.
- Replacements: faults that required eviction because no frame was empty.
- Empty frames: unused simulated frames at the end of the run.
- Hit ratio: hits divided by references.
- Page-table nodes, entries, and bytes: sparse hierarchy overhead.
- Real MINIX page size and free/cached frames before and after the run.

The relation `references = hits + page_faults` is an invariant for every
successful run. Another invariant is
`replacements = max(0, distinct-load faults after frames become full)` for the
given policy. Empty frames cannot exceed the configured frame count.

## Validation

The known-answer test uses the classic reference string
`1 2 3 4 1 2 5 1 2 3 4 5` with three frames. FIFO must produce nine faults;
LRU must produce ten. This example deliberately shows that LRU is not better
for every individual trace. The broader locality workload is expected to favor
LRU because repeated hot-set references refresh recency but do not affect FIFO
order.

The provided matrix runs 72 cases: four page sizes, three hierarchy depths,
three frame counts, and two policies. Expected interpretation includes:

1. Larger pages reduce the number of distinct pages in the fixed byte
   workload and will normally reduce faults, although they can increase
   internal fragmentation in a real system.
2. More frames cannot increase exact LRU faults for a fixed reference string.
   FIFO may violate this monotonic property on specially constructed strings.
3. LRU should outperform FIFO on the generated locality-heavy workload.
4. Hierarchy depth should primarily alter table-node and metadata costs, not
   the FIFO/LRU fault count, when all other parameters are unchanged.
5. Real MINIX free-frame values may change slightly between runs because the
   program, filesystem cache, and background services consume real memory.

## Code modification summary

`minix/commands/Makefile` includes the new command in the MINIX command build.
`etc/Makefile` includes `paging.conf` in newly generated system configuration.
The new command separates configuration parsing, trace/hierarchy/replacement
logic, and command-line/reporting logic into `config.c`, `simulator.c`, and
`vmexperiment.c`. `run_vmexperiments.sh` generates the controlled matrix, and
the `tests` directory contains the known-answer trace and test script.

## Limitations and future work

The hierarchy and replacement policies are a controlled experiment executing
inside MINIX, not a replacement for hardware address translation. Sparse-table
byte counts describe the experimental data structure rather than x86 PDE/PTE
arrays. The program measures fault counts algorithmically; it does not measure
disk swap latency because stock MINIX does not provide the required generic
anonymous-page swapping path.

A larger future implementation could add a VM-managed swap file, maintain a
reverse map from frames to process mappings, clear PTE present bits during
eviction, flush affected TLB entries, write dirty pages to swap, and reload
them through the real page-fault path. Such a change would require extensive
crash-safety and concurrency testing beyond a same-day course-project rescue.
