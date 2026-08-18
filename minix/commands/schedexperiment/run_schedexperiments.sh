#!/bin/sh

set -e

OUTPUT=${1:-/tmp/scheduling-matrix.csv}
CONFIG=/tmp/scheduling-matrix.conf
SINGLE=/tmp/scheduling-single.csv

rm -f "$OUTPUT" "$CONFIG" "$SINGLE"

for quantum in 5 10 20 40
do
	{
		echo "algorithm=ALL"
		echo "quantum_ms=$quantum"
		echo "mlfq_quanta_ms=$quantum,$((quantum * 2)),$((quantum * 4))"
		# Keep matrix runs practical on emulated 32-bit MINIX. Logical
		# scheduling metrics are independent of this physical work multiplier.
		echo "work_scale=1000"
		echo "csv_output=$SINGLE"
		echo "process=P1,0,80,3"
		echo "process=P2,10,40,4"
		echo "process=P3,20,60,0"
		echo "process=P4,35,30,2"
		echo "process=P5,50,50,1"
	} > "$CONFIG"
	schedexperiment -c "$CONFIG" >/dev/null
	if [ ! -f "$OUTPUT" ]; then
		awk -v q="$quantum" 'BEGIN { FS=OFS="," }
		    NR == 1 { print "quantum_ms," $0; next }
		    { print q, $0 }' "$SINGLE" > "$OUTPUT"
	else
		awk -v q="$quantum" 'BEGIN { FS=OFS="," }
		    NR > 1 { print q, $0 }' "$SINGLE" >> "$OUTPUT"
	fi
done

rm -f "$CONFIG" "$SINGLE"
echo "Scheduling experiment matrix written to $OUTPUT"
