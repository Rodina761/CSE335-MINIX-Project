# Native MINIX 3.3.0 results

These files were produced inside the project VirtualBox VM on 18 August 2026.

- `native-validation.txt`: OS identity, native build checks, deterministic test
  output, result line counts/checksums, and MFS extent-allocation log messages.
- `scheduling-matrix.csv`: 80 scheduling result rows.
- `paging-matrix.csv`: 72 matched FIFO/LRU result rows.
- `extent-matrix.csv`: 18 real file-I/O rows from six disposable-MFS mounts.
- `FINAL_TEST_RESULTS.md`: final clean-build, post-install test, matrix-invariant,
  and exploratory negative-test results.
- `native-clean-build-status.png`: final clean-build exit codes.
- `native-post-build-tests.png`: final install and deterministic-test evidence.
- `native-negative-test-diagnostic.png`: invalid-configuration rejection and
  the documented teardown limitation.

The extent matrix used `/dev/vnd0`, backed by a disposable 64 MiB image, and
remounted it for every `mfs_extent_size`. No system filesystem was formatted.
