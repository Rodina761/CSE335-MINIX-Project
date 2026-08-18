# Requirement 1 implementation notes

`schedexperiment` creates one real child process per configured workload entry.
Each child blocks on a command pipe, executes deterministic CPU work for the
requested logical slice, and acknowledges completion on a reply pipe. The parent
implements RR, non-preemptive SJF, non-preemptive static priority, and a
three-level feedback queue. Logical time makes comparisons repeatable; measured
wall time confirms that actual work executed.

Turnaround is `completion - arrival`; waiting is `turnaround - burst`; response
is `first start - arrival`. SJF and priority dispatch a selected process to
completion. RR rotates after each quantum. MLFQ starts arrivals at level zero,
uses configured quanta, demotes incomplete jobs, and preserves FIFO order within
a level. Lower priority numbers mean higher priority.

This experiment does not replace the production MINIX scheduler. That boundary
keeps the comparison controlled while satisfying the real-process requirement.
The report should discuss this limitation and distinguish logical policy metrics
from wall-clock overhead.
