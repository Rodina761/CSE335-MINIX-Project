CSE335 REQUIREMENT 2 - HIERARCHICAL PAGING, FIFO, AND LRU
======================================================================

1. PURPOSE AND SCOPE
--------------------

MINIX 3.3.0 on 32-bit x86 already uses hardware-defined two-level paging:
a 10-bit page-directory index, a 10-bit page-table index, and a 12-bit
offset for 4 KiB pages. The processor format cannot be safely changed to an
arbitrary page size or number of hardware levels at run time.

This project therefore adds an instrumented experiment named vmexperiment
inside the MINIX source tree. It models configurable hierarchical page-table
walks and FIFO/LRU replacement, while also querying MINIX's real VM server for
the hardware page size and total, free, largest-contiguous, and cached frame
counts. Results clearly label simulated and real measurements.

It does not claim to replace the x86 MMU or implement anonymous-page swapping,
which stock MINIX 3.3.0 does not provide as a configurable FIFO/LRU subsystem.

2. MODIFIED AND ADDED FILES
---------------------------

Modified:
  minix/commands/Makefile
  etc/Makefile

Added:
  etc/paging.conf
  minix/commands/vmexperiment/Makefile
  minix/commands/vmexperiment/vmexperiment.h
  minix/commands/vmexperiment/vmexperiment.c
  minix/commands/vmexperiment/config.c
  minix/commands/vmexperiment/simulator.c
  minix/commands/vmexperiment/run_vmexperiments.sh
  minix/commands/vmexperiment/tests/known.conf
  minix/commands/vmexperiment/tests/known.trace
  minix/commands/vmexperiment/tests/test_known.sh

3. BUILD AND INSTALL INSIDE MINIX
---------------------------------

Copy the modified source tree to /usr/src, then run as root:

  cd /usr/src/minix/commands/vmexperiment
  make
  make install
  cp /usr/src/etc/paging.conf /etc/paging.conf

For a complete system rebuild instead:

  cd /usr/src
  make build

The configuration file is part of etc/Makefile for new distributions. The
explicit cp command above is required on an already-installed system because
the MINIX build system deliberately does not overwrite /etc configuration
files during an ordinary make build.

4. CONFIGURATION
----------------

Edit /etc/paging.conf. Important fields are:

  address_bits          Width of each simulated virtual address.
  page_size             Power-of-two simulated page size in bytes.
  levels                Number of hierarchy levels (1 through 8).
  level_bits            Comma-separated index bits for every level.
  frames                Number of simulated physical frames.
  algorithm             FIFO, LRU, or BOTH.
  trace_mode            sequential, locality, random, or file.
  references            Generated reference count.
  working_set_bytes     Byte-address range used by generated traces.
  hot_bytes             Frequently accessed byte range in locality mode.
  access_stride         Byte stride used by the sequential trace.
  seed                  Deterministic pseudo-random seed.
  trace_file            Address file used when trace_mode=file.
  csv_output            Destination for machine-readable results.

The address format must satisfy:

  sum(level_bits) + log2(page_size) = address_bits

Examples for 32-bit addresses:

  4 KiB, 2 levels: level_bits=10,10
  4 KiB, 3 levels: level_bits=7,7,6
  8 KiB, 2 levels: level_bits=10,9

5. RUNNING
----------

Run the default configuration:

  vmexperiment

Use another configuration or trace:

  vmexperiment -c /path/to/config
  vmexperiment -c /etc/paging.conf -t /path/to/trace
  vmexperiment -c /etc/paging.conf -o /tmp/result.csv

Run the full 72-row comparison matrix:

  sh /usr/bin/run_vmexperiments.sh /tmp/vmexperiment-matrix.csv

6. VALIDATION
-------------

The known test uses the page reference string:

  1 2 3 4 1 2 5 1 2 3 4 5

with three frames. Correct results are 9 FIFO faults and 10 LRU faults.

  cd /usr/src/minix/commands/vmexperiment/tests
  sh test_known.sh

7. REPORTED METRICS
-------------------

For each algorithm, the CSV records references, hits, page faults,
replacements, empty frames, hit ratio, hierarchy nodes, hierarchy entries,
and approximate sparse-page-table bytes. When executed on MINIX it also
records the actual MINIX page size and free/cached frames before and after the
experiment.

FIFO evicts the page that has remained resident longest. LRU evicts the
resident page whose most recent access occurred furthest in the past. Both
algorithms receive exactly the same generated or file-based address trace,
which makes the comparison repeatable and fair. The experiment matrix keeps
the byte-address workload constant while page size changes, so changes in page
fault counts reflect page-size effects rather than a changing workload.
