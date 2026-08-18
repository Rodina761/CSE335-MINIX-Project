#!/bin/sh

set -e

OUTPUT=${1:-/tmp/extentexperiment-matrix.csv}
CONFIG=/tmp/extentexperiment-matrix.conf
SINGLE=/tmp/extentexperiment-single.csv

rm -f "$OUTPUT" "$CONFIG" "$SINGLE"

for extent in 1 2 4 8 16 32
do
	{
		echo "extent_blocks=$extent"
		echo "block_size=4096"
		echo "file_blocks=512"
		echo "iterations=3"
		echo "directory=/tmp/extentexperiment"
		echo "csv_output=$SINGLE"
	} > "$CONFIG"
	extentexperiment -c "$CONFIG" >/dev/null
	if [ ! -f "$OUTPUT" ]; then
		cat "$SINGLE" > "$OUTPUT"
	else
		tail -n +2 "$SINGLE" >> "$OUTPUT"
	fi
done

rm -f "$CONFIG" "$SINGLE"
echo "Extent experiment matrix written to $OUTPUT"
