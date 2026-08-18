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

## Exact code to study

### 1. Finding consecutive free zone bits — `cache.c:47-75`

```c
static bit_t find_zone_run_range(struct super_block *sp, bit_t first,
  bit_t limit, unsigned int length)
{
  bit_t candidate, probe;
  unsigned int offset;

  candidate = first < 1 ? 1 : first;
  while (candidate < limit && (bit_t) length <= limit - candidate) {
	for (offset = 0; offset < length; offset++) {
	  probe = candidate + (bit_t) offset;
	  if (!zone_bit_is_free(sp, probe)) break;
	}
	if (offset == length) return(candidate);
	candidate += (bit_t) offset + 1;
  }
  return(NO_BIT);
}

static bit_t find_zone_run(struct super_block *sp, bit_t origin,
  unsigned int length)
{
  bit_t map_bits, found;

  map_bits = (bit_t) (sp->s_zones - (sp->s_firstdatazone - 1));
  if (origin >= map_bits) origin = 1;
  found = find_zone_run_range(sp, origin, map_bits, length);
  if (found == NO_BIT && origin > 1)
	found = find_zone_run_range(sp, 1, origin, length);
  return(found);
}
```

Likely question: **How does the scan wrap?** It first searches from the normal
origin to the end; if unsuccessful, it searches from bit 1 back to the origin.

### 2. Connecting the run to normal allocation — `cache.c:101-119`

```c
if (z == sp->s_firstdatazone) {
	bit = sp->s_zsearch;
} else {
	bit = (bit_t) (z - (sp->s_firstdatazone - 1));
}
if (mfs_extent_size > 1 && z == sp->s_firstdatazone) {
	bit_t run;

	mfs_extent_searches++;
	run = find_zone_run(sp, bit, mfs_extent_size);
	if (run != NO_BIT) {
	  bit = run;
	  mfs_extent_hits++;
	} else {
	  mfs_extent_fallbacks++;
	}
}
b = alloc_bit(sp, ZMAP, bit);
```

This is the core modification: the new code chooses a better origin, but the
existing `alloc_bit()` remains the only operation that marks a zone allocated.

Likely question: **Where is the safe fallback?** If `find_zone_run()` returns
`NO_BIT`, `bit` is not replaced, and `alloc_bit()` receives the original search
origin.

### 3. Honoring the exact origin bit — `super.c:64-108`

```c
block = (block_t) (origin / FS_BITS_PER_BLOCK(sp->s_block_size));
word = (origin % FS_BITS_PER_BLOCK(sp->s_block_size)) / FS_BITCHUNK_BITS;
first_bit = origin % FS_BITCHUNK_BITS;

for (wptr = &b_bitmap(bp)[word]; wptr < wlim; wptr++) {
	if (*wptr == (bitchunk_t) ~0) {
		first_bit = 0;
		continue;
	}

	k = (bitchunk_t) conv4(sp->s_native, (int) *wptr);
	for (i = first_bit; i < FS_BITCHUNK_BITS &&
	    (k & (1 << i)) != 0; ++i) {}
	first_bit = 0;
	if (i == FS_BITCHUNK_BITS) continue;

	b = ((bit_t) block * FS_BITS_PER_BLOCK(sp->s_block_size))
	    + (wptr - &b_bitmap(bp)[0]) * FS_BITCHUNK_BITS
	    + i;
	if (b >= map_bits) break;

	k |= 1 << i;
	*wptr = (bitchunk_t) conv4(sp->s_native, (int) k);
	MARKDIRTY(bp);
	put_block(bp, MAP_BLOCK);
	if(map == ZMAP) {
		used_blocks++;
		lmfs_blockschange(sp->s_dev, 1);
	}
	return(b);
}
```

Likely question: **Why reset `first_bit` to zero?** The offset applies only to
the first bitmap word. Every later word must be scanned from its first bit.

### 4. Reading the MFS service option — `main.c:35-43`

```c
env_setargs(argc, argv);
mfs_extent_size = 1;
extent_size = 1;
if (env_parse("mfs_extent_size", "d", 0, &extent_size, 1, 1024) ==
    EP_SET)
	mfs_extent_size = (unsigned int) extent_size;
printf("MFS: extent allocation preference is %u zone(s)\n",
	mfs_extent_size);
```

Likely question: **What happens without the option?** The default is one zone,
which bypasses the run search and preserves ordinary behavior.

### 5. Real read-back verification — `extentexperiment.c:176-223`

```c
verify_errors = 0;
gettimeofday(&read_start, NULL);
for (offset = 0; offset < total_bytes; offset += chunk_size) {
	size_t amount;

	amount = chunk_size;
	if ((unsigned long long)amount > total_bytes - offset)
		amount = (size_t)(total_bytes - offset);
	if (read_full(fd, buffer, amount) != 0) {
		snprintf(error, error_size, "read failed: %s", strerror(errno));
		close(fd);
		unlink(filepath);
		rmdir(subdir);
		free(buffer);
		return -1;
	}
	verify_errors += verify_pattern(buffer, amount, offset, iteration);
}

fprintf(csv, "%.3f,%.3f,%u\n", throughput_mib(total_bytes, write_us),
    throughput_mib(total_bytes, read_us), verify_errors);
if (verify_errors != 0) {
	snprintf(error, error_size, "%u data verification errors",
	    verify_errors);
	return -1;
}
```

Likely question: **Why is byte verification more important than throughput?** A
fast write/read result is meaningless if the returned data is corrupted.

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
