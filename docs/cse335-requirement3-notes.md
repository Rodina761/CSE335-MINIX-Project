# Requirement 3 implementation notes

Stock MFS uses inode and zone bitmaps. `alloc_zone()` converts the preferred zone
to a bitmap bit and calls `alloc_bit()`, which searches for an available bit. The
modified path scans the zone bitmap for a configurable run when allocating the
first zone of a file, then starts the existing allocator at that exact bit.
Subsequent preferred positions naturally continue through the free run.

The policy does not reserve an entire run. Only zones actually assigned through
the original bitmap allocator become allocated. This keeps the on-disk format,
freeing logic, crash behavior, and compatibility intact. If no suitable run
exists, allocation falls back to the original search. Counters report searches,
hits, and fallbacks at unmount.

The service option is `mfs_extent_size=N` (1–1024 zones). `extentexperiment`
reads `/etc/extent.conf` and performs real directory creation, file creation,
sequential writes, `fsync`, reads with byte verification, unlink, and directory
removal. CSV records timings, throughput, logical extent count, allocated blocks,
and verification errors. Use a disposable secondary MFS mount; never use the
root file system for a destructive allocation experiment.
