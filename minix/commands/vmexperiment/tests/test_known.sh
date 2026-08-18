#!/bin/sh

# Run from this tests directory after vmexperiment is installed.

set -e

vmexperiment -c known.conf >/tmp/vmexperiment-known.out

fifo_faults=`awk -F, '$1 == "FIFO" { print $9 }' /tmp/vmexperiment-known.csv`
lru_faults=`awk -F, '$1 == "LRU" { print $9 }' /tmp/vmexperiment-known.csv`

if [ "$fifo_faults" != "9" ]; then
	echo "FAIL: expected 9 FIFO faults, got $fifo_faults" >&2
	exit 1
fi
if [ "$lru_faults" != "10" ]; then
	echo "FAIL: expected 10 LRU faults, got $lru_faults" >&2
	exit 1
fi

echo "PASS: known FIFO/LRU reference string"
