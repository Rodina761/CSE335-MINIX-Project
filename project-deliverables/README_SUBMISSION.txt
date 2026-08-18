CSE335 MINIX PROJECT — SUBMISSION PACKAGE
=========================================

Prepared: 18 August 2026
Source baseline: MINIX 3.3.0, tag baseline-v3.3.0

Files in this folder
--------------------
1. CSE335_MINIX_Project_Report.docx
   Detailed evidence-led research report; 9,137 words; visually checked at
   30 pages. It includes the theory, implementation, code maps, test cases,
   results, analysis, demonstration manuals, in-text citations, and references.
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

Native verification completed
-----------------------------
The source was built and executed inside MINIX 3.3.0/i386 on 18 August 2026.
All three deterministic tests passed. The archived raw evidence is under
native-results/: scheduling-matrix.csv (80 rows), paging-matrix.csv (72 rows),
extent-matrix.csv (18 rows), and native-validation.txt. Every extent row has
zero verification errors; MFS logged zero fallbacks for the tested run.

Reproduction commands:

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

Collect the scheduling and paging matrices:

  run_schedexperiments.sh /tmp/scheduling-matrix.csv
  run_vmexperiments.sh /tmp/paging-matrix.csv

For the extent matrix, use only a verified disposable secondary MFS device. Set
EXTENT_DEVICE and EXTENT_MOUNT_POINT before run_extent_matrix.sh; it remounts
the device with each matching mfs_extent_size. Never format or benchmark root,
/usr, or /home.

See CSE335_PROJECT_README.md at repository root for the complete procedure,
configuration reference, safety notes, modified-file map, and limitations.
