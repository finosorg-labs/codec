#!/bin/bash
# Script to merge static libraries based on .gitmodules, avoiding duplicates
# Usage: merge_libs_auto.sh <output_lib> <base_lib> <ar_command> <project_root> <build_subdir>

set -e

OUTPUT_LIB="$1"
BASE_LIB="$2"
AR_CMD="$3"
PROJECT_ROOT="$4"
BUILD_SUBDIR="${5:-linux_amd64}"  # Default to linux_amd64

# Parse .gitmodules to get submodule paths
GITMODULES="${PROJECT_ROOT}/.gitmodules"

if [ ! -f "$GITMODULES" ]; then
    echo "Error: .gitmodules not found at $GITMODULES"
    exit 1
fi

# Extract submodule paths and determine library order
# Modules are added in dependency order (leaf dependencies first)
declare -a SUBMODULE_LIBS

# Read submodule paths from .gitmodules
while IFS= read -r line; do
    if [[ "$line" =~ path[[:space:]]*=[[:space:]]*(.+) ]]; then
        submodule_path="${BASH_REMATCH[1]}"
        submodule_path=$(echo "$submodule_path" | xargs) # trim whitespace

        # Determine the library name from the submodule path
        # e.g., modules/platform -> platform
        submodule_name=$(basename "$submodule_path")

        # Find the library file for the current build subdirectory
        lib_file="${PROJECT_ROOT}/${submodule_path}/build/${BUILD_SUBDIR}/libfinkit_${submodule_name}_static.a"

        if [ -f "$lib_file" ]; then
            SUBMODULE_LIBS+=("$lib_file")
            echo "Found submodule library: $lib_file"
        else
            echo "Warning: Library not found: $lib_file"
        fi
    fi
done < "$GITMODULES"

# Reverse the array to add dependencies in correct order (deepest first)
# This ensures that when duplicates exist, the version from the leaf dependency is used
reversed_libs=()
for ((i=${#SUBMODULE_LIBS[@]}-1; i>=0; i--)); do
    reversed_libs+=("${SUBMODULE_LIBS[i]}")
done

# Create MRI script
MRI_SCRIPT=$(mktemp)
trap "rm -f $MRI_SCRIPT" EXIT

echo "CREATE $OUTPUT_LIB" > "$MRI_SCRIPT"

# Add base library first
echo "ADDLIB $BASE_LIB" >> "$MRI_SCRIPT"

# Add submodule libraries in reverse order (leaf dependencies first)
for lib in "${reversed_libs[@]}"; do
    echo "ADDLIB $lib" >> "$MRI_SCRIPT"
done

echo "SAVE" >> "$MRI_SCRIPT"
echo "END" >> "$MRI_SCRIPT"

echo ""
echo "=== Library merge order ==="
echo "1. Base library: $BASE_LIB"
i=2
for lib in "${reversed_libs[@]}"; do
    echo "$i. Submodule: $lib"
    ((i++))
done

# Execute MRI script
"$AR_CMD" -M < "$MRI_SCRIPT"

object_count=$(ar t "$OUTPUT_LIB" | wc -l)
echo ""
echo "Merged library created with $object_count objects"
echo "Note: Duplicate object names may exist across libraries (this is normal)"
