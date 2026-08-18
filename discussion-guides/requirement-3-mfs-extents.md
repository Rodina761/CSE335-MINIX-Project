# Requirement 3 — MFS extent allocation discussion guide

## What the specification asks for

Explain how MINIX manages free disk space, modify allocation to use a
user-defined extent size, configure it from a file or service option, compare
performance as extent blocks change, and understand how MINIX creates, reads,
writes, and removes files and directories.

## 30-second explanation

MFS manages free inodes and data zones with bitmaps. The project modifies the
real MFS allocation path so the first zone allocation can search for a free run
of `mfs_extent_size` zones and start normal bitmap allocation at the exact run
origin. If no run exists, MFS safely falls back to stock allocation. A native
benchmark performs real directory creation, file creation, deterministic
writes, `fsync`, reads with byte verification, unlink, and directory removal on
a disposable MFS image.

## Blocks, zones, inodes, and bitmaps

- A block is a unit of device/file-system I/O.
- An MFS zone is the allocation unit referenced by file inodes. Depending on
  file-system geometry, a zone can contain one or more blocks.
- An inode stores file metadata and direct/indirect zone references.
- The inode bitmap tracks free/used inodes.
- The zone bitmap tracks free/used data zones. A zero bit is free; allocation
  sets it and freeing clears it.
- A directory is a special file containing name-to-inode mappings.

## Architecture and code map

| File | Responsibility |
|---|---|
| `minix/fs/mfs/cache.c` | `alloc_zone()`, free-run search, hit/fallback counters |
| `minix/fs/mfs/super.c` | `alloc_bit()` exact-origin bitmap scan |
| `minix/fs/mfs/main.c` | Reads `mfs_extent_size` when the MFS service starts |
| `minix/fs/mfs/glo.h`, `proto.h` | Shared state and function declarations |
| `minix/fs/mfs/mount.c` | Reports counters at unmount |
| `etc/extent.conf` | Benchmark chunk size, file size, repetitions, output |
| `minix/commands/extentexperiment/` | Real directory and verified file-I/O benchmark |
| `run_extent_matrix.sh` | Remounts scratch MFS for sizes 1, 2, 4, 8, 16, 32 |

## What changed in the allocator

Stock `alloc_zone()` converts a preferred physical zone into a zone-bitmap bit
and calls `alloc_bit()`. The project adds `find_zone_run()` to scan the bitmap
for consecutive free bits. For the first zone of a file:

1. Increment the search counter.
2. Search for a run of the requested length, wrapping safely if necessary.
3. If found, use its first bit as the preferred origin and increment hits.
4. If not found, increment fallbacks and retain the original search origin.
5. Call the existing `alloc_bit()` to perform the actual allocation.

`super.c` was adjusted so the first bitmap word starts scanning at the exact
bit offset, rather than at bit zero of the containing word. Without this change,
MFS could find a run beginning inside a word but allocate an earlier free bit.

## Why this is safe

The run scan is read-only. It does not reserve all zones in advance. A zone is
marked allocated only when the existing allocator assigns it to the inode.
Therefore the change keeps stock accounting, freeing, and the on-disk MFS
format. If a requested run is unavailable, the original allocator still works.

The trade-off is that the feature is an extent preference, not a hard persistent
extent guarantee. Another allocation could consume part of a discovered run
before a file grows, although a quiet, serialized scratch experiment minimizes
that risk.

## Two configuration layers

The MFS service option controls placement:

```sh
mount -t mfs -o mfs_extent_size=8 /dev/vnd0 /mnt/extenttest
```

The benchmark file controls I/O:

```ini
extent_blocks=8
block_size=4096
file_blocks=512
iterations=3
directory=/mnt/extenttest/extentexperiment
csv_output=/tmp/extentexperiment.csv
```

These values must match during a controlled experiment. Changing only
`extent.conf` changes the benchmark's write chunk but does not change MFS
allocation policy. A new service option requires unmounting and remounting so a
new MFS server instance reads it.

## File and directory benchmark path

For every iteration, `extentexperiment`:

1. Creates its owned subdirectory.
2. Creates and opens `data.bin`.
3. Writes a deterministic byte pattern in configured chunks.
4. Calls `fsync()` and records allocation information with `fstat()`.
5. Seeks to the start and reads every byte.
6. Verifies the pattern and records `verify_errors`.
7. Closes and unlinks the file.
8. Removes its subdirectory.

This exercises directory entries, inode creation, zone allocation, cache
writes, persistence, reads, deallocation, and directory removal.

## Metrics and native results

The CSV records requested blocks, iteration, block and file sizes, total bytes,
logical extent count, allocated 512-byte blocks, operation times, throughput,
and verification errors.

| Requested zones | Searches | Hits | Fallbacks |
|---:|---:|---:|---:|
| 1 | 0 | 0 | 0 |
| 2 | 6 | 6 | 0 |
| 4 | 6 | 6 | 0 |
| 8 | 6 | 6 | 0 |
| 16 | 6 | 6 | 0 |
| 32 | 6 | 6 | 0 |

The native matrix used a disposable 64 MiB image through `/dev/vnd0`, produced
18 rows, and reported zero data-verification errors. Every requested run above
size 1 was found without fallback.

Some short operations measured zero microseconds because the MINIX 3.3 timer is
coarse. Do not interpret those zeros as infinite performance. Correct data and
allocator counters are stronger evidence than small throughput differences in
this run.

## Demo

Safe correctness demo:

```sh
sed -n '1,100p' /etc/extent.conf
cd /usr/src/minix/commands/extentexperiment/tests
sh test_known.sh
tail -16 /root/native-validation.txt
```

Expected final line: `PASS: extent file/directory I/O and data verification`.

Only perform a live mount demo if `/dev/vnd0` is confirmed as the disposable
image:

```sh
mount -t mfs -o mfs_extent_size=8 /dev/vnd0 /mnt/extenttest
extentexperiment -c /etc/extent.conf
umount /mnt/extenttest
```

Unmounting prints the MFS extent statistics. Never substitute the root or `/usr`
device.

## Likely questions and model answers

### How does stock MFS find free space?

It uses inode and zone bitmaps. `alloc_zone()` converts a preferred zone to a
bitmap origin, and `alloc_bit()` scans for a zero bit, sets it, marks the bitmap
buffer dirty, and updates accounting.

### Why modify both `cache.c` and `super.c`?

`cache.c` finds the start of a sufficiently long free run. `super.c` must honor
that exact bit inside the first bitmap word. Otherwise the allocator may start
at an earlier zero bit and defeat the run selection.

### Do you reserve the whole extent immediately?

No. Reserving zones without inode references could leak space after a crash and
would require new metadata and recovery rules. Existing bitmap allocation marks
each zone only when it is assigned.

### Is this a new on-disk extent-based file format?

No. Inodes still use the normal direct and indirect zone references. The change
is an extent-biased placement policy and instrumentation, preserving format
compatibility.

### What happens on fragmented free space?

If no run of the requested size exists, MFS increments the fallback counter and
uses the original allocation search. Correctness is preserved, but contiguity
is not guaranteed.

### Why use a disposable image?

Rebuilding and remounting a file-system server or formatting the wrong device
can destroy the VM. A secondary image isolates the experiment and can be reset
between cases for comparable free-space conditions.

### Why are there six searches per extent size but only three CSV rows?

Each of the three iterations creates a new subdirectory and a new data file.
Both need a first-zone allocation, so MFS performs two preferred-run searches
per iteration: six searches in total. Every search hit and none fell back.

### How would a hard extent implementation differ?

It would require persistent metadata describing extent start and length,
atomic reservation, freeing and truncation rules, crash recovery, and updates
to tools such as `fsck`. This project deliberately avoids changing the on-disk
format.

## Claims to avoid

- Do not claim the entire run is atomically reserved.
- Do not claim MFS now stores files in a new persistent extent tree.
- Do not claim zero-microsecond rows mean infinite throughput.
- Do not perform a live format or mount until the disposable device is verified.
