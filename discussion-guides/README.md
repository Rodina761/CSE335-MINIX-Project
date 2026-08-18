# CSE335 demo and discussion guide

Use this folder to divide preparation between the team. Each owner should read
the guide for their requirement, inspect the named source files, and practice
the short explanation and demo before the discussion.

| Requirement | Guide | What the owner must be able to explain |
|---|---|---|
| 1 — scheduling | [requirement-1-scheduling.md](requirement-1-scheduling.md) | RR, SJF, priority, MLFQ, real workers, metrics, and results |
| 2 — paging | [requirement-2-paging.md](requirement-2-paging.md) | hierarchy geometry, FIFO/LRU, trace control, faults, and limitations |
| 3 — MFS extents | [requirement-3-mfs-extents.md](requirement-3-mfs-extents.md) | MFS bitmaps, free-run preference, safe fallback, real I/O, and results |

## Five-minute team demo order

1. Prove the platform: `uname -a` and explain that compilation and execution
   were performed inside MINIX 3.3.0.
2. Show the three configuration files under `/etc`.
3. Run the three deterministic tests. These are the fastest correctness proof.
4. Show one CSV result from each requirement and explain one important row.
5. Show `/root/native-validation.txt` for the complete PASS summary, row counts,
   checksums, and MFS counters.

```sh
uname -a

cd /usr/src/minix/commands/schedexperiment/tests && sh test_known.sh
cd /usr/src/minix/commands/vmexperiment/tests && sh test_known.sh
cd /usr/src/minix/commands/extentexperiment/tests && sh test_known.sh

sed -n '1,80p' /etc/scheduler.conf
sed -n '1,80p' /etc/paging.conf
sed -n '1,80p' /etc/extent.conf

sed -n '1,30p' /root/native-validation.txt
```

Expected test output:

```text
PASS: known scheduling workload
PASS: known FIFO/LRU reference string
PASS: extent file/directory I/O and data verification
```

## Facts every team member should know

- The baseline is the official MINIX 3.3.0 source, tagged
  `baseline-v3.3.0` in Git.
- Requirement 1 is a controlled user-space dispatcher running real MINIX child
  processes. It does not replace the production kernel scheduler.
- Requirement 2 is a configurable paging and replacement model compiled and
  run natively in MINIX. It does not reconfigure the x86 MMU.
- Requirement 3 modifies the real MFS zone-allocation path.
- Every experiment reads a text configuration and writes CSV evidence.
- Native evidence contains 80 scheduling rows, 72 paging rows, and 18 extent
  rows. The validation file has 173 total CSV lines because it includes three
  headers.
- The extent benchmark reported zero verification errors. MFS found every
  requested run from 2 through 32 zones with zero fallbacks in the native run.

## Questions likely to be asked to the whole group

### Why did you start from MINIX 3.3.0 rather than the current GitHub branch?

The VM and course project target MINIX 3.3.0. A newer source tree can have
different headers, server interfaces, and build files. Matching the source to
the running image prevents version-mismatch errors.

### How do you prove the results were not invented on Windows?

The commands and modified MFS server were built with the native MINIX toolchain.
The repository contains the raw CSV files, native test output, OS identity,
matrix checksums, and VirtualBox console screenshots.

### What is the difference between correctness and performance tests?

The known tests have exact expected answers or byte verification and must pass
first. The matrices then vary configuration values and collect performance
metrics. A fast run with a wrong expected result is still a failure.

### What would you improve with more time?

- Repeat wall-clock measurements and report medians and ranges.
- Add larger and more varied workloads.
- Integrate scheduling policies into the production scheduling server.
- Add a real swap-backed VM replacement path for process pages.
- Add persistent extent metadata if hard extent guarantees are required.

## Demo safety

- Take a VM snapshot before replacing or remounting MFS.
- Never format the root or `/usr` device.
- Requirement 3 should use only the disposable `/dev/vnd0` image.
- If time is short, show the archived native validation instead of rebuilding
  MFS live.
- Do not say a simulated page fault is a hardware page fault, a logical
  dispatch is a kernel context switch, or an allocation preference is a new
  persistent extent file format.

## Last-minute checklist

- [ ] Each owner can explain their requirement without reading the report.
- [ ] Each owner can point to the main source function and configuration file.
- [ ] Each owner can explain one result and one limitation.
- [ ] The VM boots and the three known tests are available.
- [ ] Native CSV files and `native-validation.txt` are easy to locate.
- [ ] The team knows who will control the VM during the demonstration.
