#!/bin/sh

set -e

extentexperiment -c known.conf >/tmp/extentexperiment-known.out

rows=`awk 'END { print NR - 1 }' /tmp/extentexperiment-known.csv`
errors=`awk -F, 'NR > 1 { total += $14 } END { print total + 0 }' \
    /tmp/extentexperiment-known.csv`

if [ "$rows" != "2" ]; then
	echo "FAIL: expected 2 result rows, got $rows" >&2
	exit 1
fi
if [ "$errors" != "0" ]; then
	echo "FAIL: expected zero verification errors, got $errors" >&2
	exit 1
fi

echo "PASS: extent file/directory I/O and data verification"
