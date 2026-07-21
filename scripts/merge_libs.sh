#!/bin/bash
# Script to merge static libraries using MRI script
# Usage: merge_libs.sh <output_lib> <ar_command> <lib1> <lib2> ...

set -e

OUTPUT_LIB="$1"
AR_CMD="$2"
shift 2

# Create MRI script
MRI_SCRIPT=$(mktemp)
trap "rm -f $MRI_SCRIPT" EXIT

echo "CREATE $OUTPUT_LIB" > "$MRI_SCRIPT"
for lib in "$@"; do
    echo "ADDLIB $lib" >> "$MRI_SCRIPT"
done
echo "SAVE" >> "$MRI_SCRIPT"
echo "END" >> "$MRI_SCRIPT"

# Execute MRI script
"$AR_CMD" -M < "$MRI_SCRIPT"

echo "Merged library created with $(ar t "$OUTPUT_LIB" | wc -l) objects"
