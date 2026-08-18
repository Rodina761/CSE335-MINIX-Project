#!/bin/sh

# Run the Requirement 2 experiment matrix. Each case runs FIFO and LRU over
# the same deterministic locality trace. The final file contains 72 rows:
# 4 page sizes x 3 hierarchy depths x 3 frame counts x 2 algorithms.

set -e

OUTPUT=${1:-/tmp/vmexperiment-matrix.csv}
CONFIG=/tmp/vmexperiment-matrix.conf
SINGLE=/tmp/vmexperiment-single.csv

rm -f "$OUTPUT" "$CONFIG" "$SINGLE"

run_case()
{
	page_size=$1
	levels=$2
	level_bits=$3
	frames=$4

	{
		echo "address_bits=32"
		echo "page_size=$page_size"
		echo "levels=$levels"
		echo "level_bits=$level_bits"
		echo "frames=$frames"
		echo "algorithm=BOTH"
		echo "trace_mode=locality"
		echo "references=10000"
		echo "working_set_bytes=1048576"
		echo "hot_bytes=131072"
		echo "access_stride=64"
		echo "seed=335"
		echo "csv_output=$SINGLE"
	} > "$CONFIG"

	vmexperiment -c "$CONFIG" >/dev/null
	if [ ! -f "$OUTPUT" ]; then
		cat "$SINGLE" > "$OUTPUT"
	else
		tail -n +2 "$SINGLE" >> "$OUTPUT"
	fi
}

for frames in 16 32 64
do
	run_case 1024 1 22 "$frames"
	run_case 1024 2 11,11 "$frames"
	run_case 1024 3 8,7,7 "$frames"
	run_case 2048 1 21 "$frames"
	run_case 2048 2 11,10 "$frames"
	run_case 2048 3 7,7,7 "$frames"
	run_case 4096 1 20 "$frames"
	run_case 4096 2 10,10 "$frames"
	run_case 4096 3 7,7,6 "$frames"
	run_case 8192 1 19 "$frames"
	run_case 8192 2 10,9 "$frames"
	run_case 8192 3 7,6,6 "$frames"
done

rm -f "$CONFIG" "$SINGLE"
echo "Experiment matrix written to $OUTPUT"
