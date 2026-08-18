#!/bin/sh

set -e

OUTPUT=${1:-/tmp/extentexperiment-matrix.csv}
CONFIG=/tmp/extentexperiment-matrix.conf
SINGLE=/tmp/extentexperiment-single.csv
DEVICE=${EXTENT_DEVICE:-}
MOUNT_POINT=${EXTENT_MOUNT_POINT:-/mnt/extenttest}
mounted=0

cleanup()
{
	if [ "$mounted" -eq 1 ]; then
		umount "$MOUNT_POINT" 2>/dev/null || true
	fi
}
trap cleanup 0 1 2 15

rm -f "$OUTPUT" "$CONFIG" "$SINGLE"

for extent in 1 2 4 8 16 32
do
	if [ -n "$DEVICE" ]; then
		mkdir -p "$MOUNT_POINT"
		mount -t mfs -o "mfs_extent_size=$extent" "$DEVICE" "$MOUNT_POINT"
		mounted=1
		directory="$MOUNT_POINT/extentexperiment"
	else
		directory=/tmp/extentexperiment
	fi
	{
		echo "extent_blocks=$extent"
		echo "block_size=4096"
		echo "file_blocks=512"
		echo "iterations=3"
		echo "directory=$directory"
		echo "csv_output=$SINGLE"
	} > "$CONFIG"
		extentexperiment -c "$CONFIG" >/dev/null
	if [ -n "$DEVICE" ]; then
		umount "$MOUNT_POINT"
		mounted=0
	fi
	if [ ! -f "$OUTPUT" ]; then
		cat "$SINGLE" > "$OUTPUT"
	else
		tail -n +2 "$SINGLE" >> "$OUTPUT"
	fi
done

rm -f "$CONFIG" "$SINGLE"
echo "Extent experiment matrix written to $OUTPUT"
