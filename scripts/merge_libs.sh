#!/bin/bash
# Static library merge script with library-level deduplication
# Merges base library with all its dependencies into a single library file
#
# Assumptions:
#   - Each finkit module's static library already bundles its dependencies
#   - Dependencies should not be added separately to avoid duplication
#
# Usage: merge_libs.sh <output_lib> <base_lib> <ar_command> [options]
#
# Options:
#   --project-root=<path>       Project root directory (required)
#   --build-subdir=<name>       Build subdirectory name (default: linux_amd64)

set -e

# Parse arguments
OUTPUT_LIB=""
BASE_LIB=""
AR_CMD=""
PROJECT_ROOT=""
BUILD_SUBDIR="linux_amd64"

while [[ $# -gt 0 ]]; do
    case $1 in
        --project-root=*)
            PROJECT_ROOT="${1#*=}"
            shift
            ;;
        --build-subdir=*)
            BUILD_SUBDIR="${1#*=}"
            shift
            ;;
        *)
            if [ -z "$OUTPUT_LIB" ]; then
                OUTPUT_LIB="$1"
            elif [ -z "$BASE_LIB" ]; then
                BASE_LIB="$1"
            elif [ -z "$AR_CMD" ]; then
                AR_CMD="$1"
            fi
            shift
            ;;
    esac
done

# Validate arguments
if [ -z "$OUTPUT_LIB" ] || [ -z "$BASE_LIB" ] || [ -z "$AR_CMD" ]; then
    echo "Error: Missing required arguments"
    echo "Usage: merge_libs.sh <output_lib> <base_lib> <ar_command> --project-root=<path> [--build-subdir=<name>]"
    exit 1
fi

if [ -z "$PROJECT_ROOT" ]; then
    echo "Error: --project-root required"
    exit 1
fi

# Parse .gitmodules and extract submodule names
parse_gitmodules() {
    local gitmodules_file="$1"
    local -n result=$2

    [ ! -f "$gitmodules_file" ] && return

    while IFS= read -r line; do
        if [[ "$line" =~ path[[:space:]]*=[[:space:]]*(.+) ]]; then
            submodule_path=$(echo "${BASH_REMATCH[1]}" | tr -d '\r' | xargs)
            result+=("$(basename "$submodule_path")")
        fi
    done < "$gitmodules_file"
}

# Find library file for a module
find_module_lib() {
    local mod="$1"
    local lib_name="libfinkit_${mod}_static.a"

    # Try build directory first (pre-built dependencies)
    [ -f "${PROJECT_ROOT}/build/${BUILD_SUBDIR}/${lib_name}" ] && \
        echo "${PROJECT_ROOT}/build/${BUILD_SUBDIR}/${lib_name}" && return

    # Try modules directory (locally built submodules)
    [ -f "${PROJECT_ROOT}/modules/${mod}/build/${BUILD_SUBDIR}/${lib_name}" ] && \
        echo "${PROJECT_ROOT}/modules/${mod}/build/${BUILD_SUBDIR}/${lib_name}" && return

    return 1
}

declare -a merge_order

# Get current project name from base library
CURRENT_PROJECT=""
base_name=$(basename "$BASE_LIB")
if [[ "$base_name" =~ libfinkit_([^_]+)_static ]]; then
    CURRENT_PROJECT="${BASH_REMATCH[1]}"
fi

echo "=== Dependency-aware merge ==="
[ -n "$CURRENT_PROJECT" ] && echo "Current project: $CURRENT_PROJECT"

# Parse root .gitmodules
GITMODULES="${PROJECT_ROOT}/.gitmodules"
if [ ! -f "$GITMODULES" ]; then
    echo "Warning: .gitmodules not found, merging base library only"
else
    declare -a root_modules
    parse_gitmodules "$GITMODULES" root_modules
    echo "Root modules: ${root_modules[@]}"

    # Scan third_party directory for libraries
    THIRD_PARTY_DIR="${PROJECT_ROOT}/build/${BUILD_SUBDIR}/third_party"
    if [ -d "$THIRD_PARTY_DIR" ]; then
        for lib_file in "$THIRD_PARTY_DIR"/*.a; do
            [ -f "$lib_file" ] && merge_order+=("$lib_file")
        done
    fi

    # Collect direct dependencies and build dependency graph
    declare -A module_deps  # mod -> "dep1 dep2 ..."
    declare -a candidates   # modules that might be added

    for mod in "${root_modules[@]}"; do
        [ "$mod" = "$CURRENT_PROJECT" ] && continue

        lib_file=$(find_module_lib "$mod" || true)
        if [ -n "$lib_file" ]; then
            candidates+=("$mod")

            # Check if this module has dependencies
            mod_gitmodules="${PROJECT_ROOT}/modules/${mod}/.gitmodules"
            if [ -f "$mod_gitmodules" ]; then
                declare -a sub_deps
                parse_gitmodules "$mod_gitmodules" sub_deps
                if [ ${#sub_deps[@]} -gt 0 ]; then
                    module_deps["$mod"]="${sub_deps[@]}"
                fi
            fi
        fi
    done

    # Filter out modules that are dependencies of other modules
    # If A depends on B, and both A and B are in candidates, only add A (since A bundles B)
    for mod in "${candidates[@]}"; do
        is_bundled=false

        # Check if this module is a dependency of any other candidate
        for other_mod in "${candidates[@]}"; do
            if [ "$mod" != "$other_mod" ] && [ -n "${module_deps[$other_mod]}" ]; then
                # Check if mod is in other_mod's dependencies
                for dep in ${module_deps[$other_mod]}; do
                    if [ "$dep" = "$mod" ]; then
                        echo "  Skipping $mod (bundled in $other_mod)"
                        is_bundled=true
                        break 2
                    fi
                done
            fi
        done

        if [ "$is_bundled" = false ]; then
            lib_file=$(find_module_lib "$mod" || true)
            [ -n "$lib_file" ] && merge_order+=("$lib_file")
        fi
    done
fi

# Display merge order
echo ""
echo "=== Final merge order ==="
echo "1. Base: $(basename "$BASE_LIB")"
i=2
for lib in "${merge_order[@]}"; do
    echo "$i. $(basename "$lib")"
    i=$((i + 1))
done

# Execute merge based on tool type
echo ""
if [[ "$AR_CMD" == *"lib.exe"* ]]; then
    echo "Using Windows lib.exe..."
    "$AR_CMD" /OUT:"$OUTPUT_LIB" "$BASE_LIB" "${merge_order[@]}"
elif [[ "$AR_CMD" == *"libtool"* ]]; then
    echo "Using macOS libtool..."
    "$AR_CMD" -static -o "$OUTPUT_LIB" "$BASE_LIB" "${merge_order[@]}" 2>&1 | grep -v "warning duplicate member name" || true
else
    echo "Using ar with MRI script..."
    MRI_SCRIPT=$(mktemp)
    trap "rm -f $MRI_SCRIPT" EXIT

    {
        echo "CREATE $OUTPUT_LIB"
        echo "ADDLIB $BASE_LIB"
        for lib in "${merge_order[@]}"; do
            echo "ADDLIB $lib"
        done
        echo "SAVE"
        echo "END"
    } > "$MRI_SCRIPT"

    "$AR_CMD" -M < "$MRI_SCRIPT"
fi

object_count=$(ar t "$OUTPUT_LIB" 2>/dev/null | wc -l || echo "N/A")
echo "✓ Merged library created with $object_count objects"
