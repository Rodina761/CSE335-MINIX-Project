# Modified source-code proof

This project is compared against the official MINIX 3.3.0 baseline at tag
`baseline-v3.3.0`.

The complete unified diff is available in
`project-deliverables/CSE335_MINIX_SOURCE_CHANGES.patch`. Lines beginning with
`+` were added; lines beginning with `-` were removed.

## Existing MINIX files modified

- `etc/Makefile`
- `minix/commands/Makefile`
- `minix/fs/mfs/cache.c`
- `minix/fs/mfs/glo.h`
- `minix/fs/mfs/main.c`
- `minix/fs/mfs/mount.c`
- `minix/fs/mfs/proto.h`
- `minix/fs/mfs/super.c`

## Requirement 1 files added

- `etc/scheduler.conf`
- `minix/commands/schedexperiment/Makefile`
- `minix/commands/schedexperiment/config.c`
- `minix/commands/schedexperiment/run_schedexperiments.sh`
- `minix/commands/schedexperiment/schedexperiment.c`
- `minix/commands/schedexperiment/schedexperiment.h`
- `minix/commands/schedexperiment/scheduler.c`
- `minix/commands/schedexperiment/tests/known.conf`
- `minix/commands/schedexperiment/tests/test_known.sh`

## Requirement 2 files added

- `etc/paging.conf`
- `minix/commands/vmexperiment/Makefile`
- `minix/commands/vmexperiment/config.c`
- `minix/commands/vmexperiment/run_vmexperiments.sh`
- `minix/commands/vmexperiment/simulator.c`
- `minix/commands/vmexperiment/tests/known.conf`
- `minix/commands/vmexperiment/tests/known.trace`
- `minix/commands/vmexperiment/tests/test_known.sh`
- `minix/commands/vmexperiment/vmexperiment.c`
- `minix/commands/vmexperiment/vmexperiment.h`

## Requirement 3 files added

- `etc/extent.conf`
- `minix/commands/extentexperiment/Makefile`
- `minix/commands/extentexperiment/config.c`
- `minix/commands/extentexperiment/extentexperiment.c`
- `minix/commands/extentexperiment/extentexperiment.h`
- `minix/commands/extentexperiment/run_extent_matrix.sh`
- `minix/commands/extentexperiment/tests/known.conf`
- `minix/commands/extentexperiment/tests/test_known.sh`

## Verified totals

- 35 files added or modified
- 2,784 inserted lines
- 9 deleted lines
