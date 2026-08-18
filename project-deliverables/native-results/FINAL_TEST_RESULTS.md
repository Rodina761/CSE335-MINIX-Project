# Final native test results

Test date: 18 August 2026

Platform: MINIX 3.3.0 (GENERIC), i386, running in Oracle VirtualBox.

## Outcome

All required clean builds, command installations, deterministic functional
tests, and archived-result invariants passed. The final post-build tests were
run after rebuilding and reinstalling the three experiment commands, so they
tested the current source rather than only an older installed binary.

| Check | Result |
|---|---|
| Clean build: `schedexperiment` | PASS, exit 0 |
| Clean build: `vmexperiment` | PASS, exit 0 |
| Clean build: `extentexperiment` | PASS, exit 0 |
| Clean build: modified MFS server | PASS, exit 0 |
| Install: `schedexperiment` | PASS, exit 0 |
| Install: `vmexperiment` | PASS, exit 0 |
| Install: `extentexperiment` | PASS, exit 0 |
| Known scheduling workload | PASS, exit 0 |
| Known FIFO/LRU reference string | PASS, exit 0 |
| Extent file/directory I/O and byte verification | PASS, exit 0 |
| Scheduling matrix formulas and coverage | PASS |
| Paging matrix accounting and coverage | PASS |
| Extent matrix byte counts and verification | PASS |
| Archived CSV sizes against native validation log | PASS |
| Six project shell scripts parse cleanly | PASS |
| Report-generation Python syntax | PASS |
| DOCX package and 8,000-word minimum | PASS |
| PDF package, 28-page count, and marker scan | PASS |
| PPTX package, 12-slide count, and requirement coverage | PASS |
| Git whitespace validation | PASS |

## Deterministic assertions

The scheduling test checks exact average turnaround and waiting times:

| Policy | Expected turnaround | Expected waiting |
|---|---:|---:|
| RR | 185.00 ms | 133.00 ms |
| SJF | 137.00 ms | 85.00 ms |
| Priority | 155.00 ms | 103.00 ms |
| MLFQ | 191.00 ms | 139.00 ms |

The paging test checks an exact known reference string and requires nine FIFO
faults and ten LRU faults. This case intentionally demonstrates that LRU is not
guaranteed to beat FIFO for every individual trace.

The extent test performs two real create/write/read/remove iterations. It
requires two CSV rows and a total `verify_errors` value of zero.

## Archived matrix assertions

- Scheduling: 80 rows; RR, SJF, Priority, and MLFQ are all present. Every row
  satisfies `turnaround = completion - arrival`, `waiting = turnaround - burst`,
  and `response = start - arrival`.
- Paging: 72 rows; FIFO and LRU are both present. Every row satisfies
  `references = hits + page_faults`, `replacements <= page_faults`, valid empty
  frame bounds, and a matching hit ratio.
- Extents: 18 rows across preferences 1, 2, 4, 8, 16, and 32. Every row
  satisfies `bytes = block_size * file_blocks`; the sum of verification errors
  is zero.
- The three archived CSV byte sizes match the values recorded by the original
  native validation log: 5,385, 6,398, and 1,229 bytes.

The six project test/matrix shell scripts pass `bash -n`. The report tools pass
Python syntax parsing, `git diff --check` reports no whitespace errors, the DOCX
is a valid Office package above 8,000 words, the cleaned PDF contains 28 pages,
and the 12-slide PowerPoint covers all three requirements.

## Commands used in MINIX

```sh
cd /usr/src/minix/commands/schedexperiment && make clean && make && make install
cd /usr/src/minix/commands/vmexperiment && make clean && make && make install
cd /usr/src/minix/commands/extentexperiment && make clean && make && make install
cd /usr/src/minix/fs/mfs && make clean && make

cd /usr/src/minix/commands/schedexperiment/tests && sh test_known.sh
cd /usr/src/minix/commands/vmexperiment/tests && sh test_known.sh
cd /usr/src/minix/commands/extentexperiment/tests && sh test_known.sh
```

## Exploratory negative test

An invalid scheduling configuration with `quantum_ms=0` produced the correct
validation message: `quantum_ms and work_scale must be greater than zero`.
However, the process did not finish its teardown on this VM image and required
Ctrl+C. This does not affect the successful normal test suite, but it is an
error-path limitation worth stating honestly. It is not counted as a passing
test above.

The console also emitted `SYSTEM: denied request` messages during some command
shutdowns. The recorded build, install, and functional-test exit codes were all
zero, and all deterministic assertions passed.

## Evidence

- `native-clean-build-status.png`: all four clean-build exit codes.
- `native-post-build-tests.png`: three install exits, three test exits, and the
  three PASS messages after installation.
- `native-negative-test-diagnostic.png`: invalid configuration rejection and
  the documented teardown limitation.
- `native-validation.txt`: original native identity, tests, checksums, row
  totals, and MFS counters.
