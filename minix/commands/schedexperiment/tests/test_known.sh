#!/bin/sh

set -e

schedexperiment -c known.conf >/tmp/schedexperiment-known.out

check_average()
{
	algorithm=$1
	expected_turnaround=$2
	expected_waiting=$3
	actual=`awk -F, -v a="$algorithm" '$1 == a { print $11 "," $12; exit }' \
	    /tmp/schedexperiment-known.csv`
	if [ "$actual" != "$expected_turnaround,$expected_waiting" ]; then
		echo "FAIL: $algorithm expected $expected_turnaround,$expected_waiting got $actual" >&2
		exit 1
	fi
}

check_average RR 185.00 133.00
check_average SJF 137.00 85.00
check_average PRIORITY 155.00 103.00
check_average MLFQ 191.00 139.00

echo "PASS: known scheduling workload"
