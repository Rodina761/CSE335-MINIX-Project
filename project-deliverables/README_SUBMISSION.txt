CSE335 MINIX PROJECT — SUBMISSION PACKAGE
=========================================

Prepared: 18 August 2026
Source baseline: MINIX 3.3.0, tag baseline-v3.3.0

Files in this folder
--------------------
1. CSE335_MINIX_Project_Report.docx
   Editable detailed report; 9,956 words; visually checked at 27 pages.
2. CSE335_MINIX_Project_Report.pdf
   Rendered reference copy of the report.
3. CSE335_MINIX_Project_Presentation.pptx
   Editable 12-slide presentation; every slide rendered and checked.
4. CSE335_MINIX_Modified_Source.zip
   Source snapshot created from the final Git commit (added after this note).

Project requirements
--------------------
Requirement 1: RR, SJF, priority, and MLFQ with real child workers — implemented.
Requirement 2: configurable hierarchy/page size plus FIFO/LRU — implemented.
Requirement 3: MFS extent-biased allocation plus real I/O benchmark — implemented.

Mandatory native verification before final upload
-------------------------------------------------
The source was prepared and statically reviewed on Windows. Run these commands
inside the bootable MINIX 3.3 VM; do not call host-side oracle numbers measured
MINIX results.

  cd /usr/src
  make includes
  cd /usr/src/minix/commands/schedexperiment && make && make install
  cd /usr/src/minix/commands/vmexperiment && make && make install
  cd /usr/src/minix/commands/extentexperiment && make && make install
  cd /usr/src/minix/fs/mfs && make && make install
  cp /usr/src/etc/scheduler.conf /etc/scheduler.conf
  cp /usr/src/etc/paging.conf /etc/paging.conf
  cp /usr/src/etc/extent.conf /etc/extent.conf

  cd /usr/src/minix/commands/schedexperiment/tests && sh test_known.sh
  cd /usr/src/minix/commands/vmexperiment/tests && sh test_known.sh
  cd /usr/src/minix/commands/extentexperiment/tests && sh test_known.sh

Then collect:

  run_schedexperiments.sh /tmp/scheduling-matrix.csv
  run_vmexperiments.sh /tmp/paging-matrix.csv

For the extent matrix, use only a verified disposable secondary MFS device.
Mount it with mfs_extent_size matching extent_blocks, point extent.conf under
that mount, then run run_extent_matrix.sh. Never format or benchmark the root or
/usr device.

See CSE335_PROJECT_README.md at repository root for the complete procedure,
configuration reference, safety notes, modified-file map, and limitations.
