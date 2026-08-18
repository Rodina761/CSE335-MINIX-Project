# CSE335 Operating Systems Project — MINIX 3.3.0

This repository starts from the official MINIX 3.3.0 source release and adds
three reproducible experiments required by the Summer 2026 project:

1. real-process CPU scheduling with RR, SJF, priority, and MLFQ;
2. configurable hierarchical paging with FIFO and LRU replacement; and
3. extent-aware MFS allocation plus a real file-system I/O benchmark.

The Git tag `baseline-v3.3.0` identifies the untouched source baseline.  Each
experiment reads a text configuration file and writes machine-readable CSV.

## Important interpretation

MINIX 3.3.0 already contains a production scheduler, VM server, and MFS file
server. Replacing all three production subsystems at once would make controlled
comparison unsafe and irreproducible. The project therefore uses two techniques:

- Requirements 1 and 2 are controlled user-space experiment drivers compiled
  and executed inside MINIX. Requirement 1 dispatches real child processes.
  Requirement 2 builds real configurable page-table structures and also records
  actual MINIX VM counters through `vm_info_stats()`.
- Requirement 3 changes the real MFS zone-allocation path. The MFS server accepts
  a preferred run length and selects the start of a sufficiently long free run;
  normal bitmap allocation then preserves consistency and falls back safely.

The configurable page size and hierarchy describe the experiment's virtual
address model. They do not reprogram x86 MMU hardware: the running MINIX kernel
continues to use its hardware-supported page size.

## Modified and added files

### Requirement 1 — scheduling

- `minix/commands/schedexperiment/`: parser, scheduler, real child workers, test
- `etc/scheduler.conf`: algorithms, quanta, workload, output path
- `minix/commands/Makefile` and `etc/Makefile`: build/install integration

Metrics include per-process start/completion/turnaround/waiting/response times,
averages, logical makespan, dispatch count, and measured wall time.

### Requirement 2 — paging

- `minix/commands/vmexperiment/`: hierarchy, trace generator, FIFO/LRU, test
- `etc/paging.conf`: address width, page size, levels, level bits, frames, trace
- `minix/commands/vmexperiment/run_vmexperiments.sh`: controlled matrix

Metrics include faults, hits, replacements, empty frames, hierarchy nodes,
entries and bytes, plus MINIX VM page-size/free/cache counters when available.

### Requirement 3 — MFS extents

- `minix/fs/mfs/cache.c`: free-run search and extent-biased zone allocation
- `minix/fs/mfs/super.c`: exact-origin bitmap allocation
- `minix/fs/mfs/main.c`, `glo.h`, `proto.h`, `mount.c`: configuration/statistics
- `minix/commands/extentexperiment/`: real create/write/read/remove benchmark
- `etc/extent.conf`: extent and benchmark parameters

The on-disk MFS format is intentionally unchanged. Extents are an allocation
policy: no zones are marked allocated until the existing allocator assigns them.
This prevents leaked reservations and keeps existing file systems compatible.

## Build inside MINIX

Place this source tree at `/usr/src`, log in as root, and run:

```sh
cd /usr/src
make includes
cd /usr/src/minix/commands/schedexperiment && make && make install
cd /usr/src/minix/commands/vmexperiment && make && make install
cd /usr/src/minix/commands/extentexperiment && make && make install
cp /usr/src/etc/scheduler.conf /etc/scheduler.conf
cp /usr/src/etc/paging.conf /etc/paging.conf
cp /usr/src/etc/extent.conf /etc/extent.conf
```

Build and install the modified MFS server before the extent experiment:

```sh
cd /usr/src/minix/fs/mfs
make && make install
```

Use a disposable secondary MFS file system for extent tests. Do not experiment
on the root file system. After identifying a scratch device, for example
`/dev/c0d1p0`, mount it with the selected preference:

```sh
mkdir -p /mnt/extenttest
mount -t mfs -o mfs_extent_size=8 /dev/c0d1p0 /mnt/extenttest
```

Set `directory=/mnt/extenttest/extentexperiment` in `/etc/extent.conf`. Unmounting
prints MFS search/hit/fallback counters. Change `mfs_extent_size` only by
unmounting and mounting the scratch file system again.

## Verification and experiments

Run deterministic smoke tests:

```sh
cd /usr/src/minix/commands/schedexperiment/tests && sh test_known.sh
cd /usr/src/minix/commands/vmexperiment/tests && sh test_known.sh
cd /usr/src/minix/commands/extentexperiment/tests && sh test_known.sh
```

Run the default configurations:

```sh
schedexperiment
vmexperiment
extentexperiment
```

Run controlled matrices:

```sh
run_schedexperiments.sh /tmp/scheduling-matrix.csv
run_vmexperiments.sh /tmp/paging-matrix.csv
run_extent_matrix.sh /tmp/extent-matrix.csv
```

Copy the three CSV files and the console output from the VM to the submission
folder. The report must distinguish measured MINIX results from any expected or
host-side oracle values.

## Configuration reference

`scheduler.conf` supports `algorithm=ALL|RR|SJF|PRIORITY|MLFQ`, `quantum_ms`,
`mlfq_quanta_ms`, `work_scale`, `csv_output`, and repeated
`process=name,arrival_ms,burst_ms,priority` lines. Lower numeric priority wins.

`paging.conf` supports `address_bits`, power-of-two `page_size`, `levels`,
`level_bits`, `frames`, `algorithm=FIFO|LRU|BOTH`, trace mode and trace controls.
Hierarchy bits plus offset bits must exactly equal address bits.

`extent.conf` supports `extent_blocks`, `block_size`, `file_blocks`, `iterations`,
`directory`, and `csv_output`. Keep the directory on the scratch MFS mount.

## Safety and reproducibility

- Snapshot the VM before installing or remounting MFS.
- Never format or use a device until its identity has been verified.
- Use the same workload, frame count, and byte-address trace across paging runs.
- Use the same file size and VM conditions across extent runs.
- Repeat performance runs and report variability; never invent missing results.
- Restore configuration files after each matrix run.

## Repository history

- `baseline-v3.3.0`: unmodified MINIX 3.3.0
- later commits: one requirement-focused, reviewable change set at a time

See `PROJECT_STATUS.md` for the submission checklist and the detailed report for
design rationale, source walkthroughs, method, results, limitations, and analysis.
