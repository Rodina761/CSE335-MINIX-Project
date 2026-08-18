CSE335 MINIX 3.3.0 - CHANGED SOURCE FILES
==========================================

This archive contains only the files added or modified relative to the official
MINIX 3.3.0 baseline. Paths are preserved so the archive can be extracted over
a clean MINIX 3.3.0 source tree.

Change summary
--------------
35 files changed, 2,784 insertions, 9 deletions.

Requirement 1 - scheduling experiment
-------------------------------------
minix/commands/schedexperiment/
etc/scheduler.conf
minix/commands/Makefile
etc/Makefile

Requirement 2 - hierarchical paging and FIFO/LRU experiment
------------------------------------------------------------
minix/commands/vmexperiment/
etc/paging.conf
minix/commands/Makefile
etc/Makefile

Requirement 3 - MFS extent-aware allocation and benchmark
-----------------------------------------------------------
minix/commands/extentexperiment/
minix/fs/mfs/cache.c
minix/fs/mfs/glo.h
minix/fs/mfs/main.c
minix/fs/mfs/mount.c
minix/fs/mfs/proto.h
minix/fs/mfs/super.c
etc/extent.conf
minix/commands/Makefile
etc/Makefile

How to use this focused archive
-------------------------------
1. Start with an official MINIX 3.3.0 source tree.
2. Extract this ZIP at the source root, preserving the included paths.
3. On MINIX, ensure the experiment scripts are executable:

   chmod +x minix/commands/schedexperiment/*.sh
   chmod +x minix/commands/schedexperiment/tests/*.sh
   chmod +x minix/commands/vmexperiment/*.sh
   chmod +x minix/commands/vmexperiment/tests/*.sh
   chmod +x minix/commands/extentexperiment/*.sh
   chmod +x minix/commands/extentexperiment/tests/*.sh

4. Build and install as documented in the full submission README.

The LMS deliverable still uses CSE335_MINIX_Modified_Source.zip because the
assignment requests the complete modified MINIX version. This smaller archive
is an inspection bundle that makes every project-specific change easy to find.
