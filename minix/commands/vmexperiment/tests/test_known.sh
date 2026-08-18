#!/bin/sh

# Run from this tests directory after vmexperiment is installed.

set -e

rm -f /tmp/vmexperiment-known.csv
vmexperiment -c known.conf >/tmp/vmexperiment-known.out 2>/tmp/vmexperiment-known.err &
vm_pid=$!
attempt=0
while [ "$attempt" -lt 30 ]
do
	if [ -f /tmp/vmexperiment-known.csv ] &&
	    [ "`wc -l < /tmp/vmexperiment-known.csv`" -ge 3 ]; then
		break
	fi
	sleep 1
	attempt=`expr "$attempt" + 1`
done
kill -9 "$vm_pid" 2>/dev/null || true
wait "$vm_pid" 2>/dev/null || true

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
