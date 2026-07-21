#!/bin/bash
# Universal static library merge script with intelligent dependency resolution
# Supports three modes:
#   1. Manual mode: explicitly list libraries to merge
#   2. Auto mode: read from .gitmodules (include all)
#   3. Smart mode: read from .gitmodules with intelligent deduplication (default)
#
# Usage: merge_libs.sh <output_lib> <base_lib> <ar_command> [options]
#
# Options:
#   --mode=manual|auto|smart    Merge mode (default: smart)
#   --project-root=<path>       Project root directory (required for auto/smart mode)
#   --build-subdir=<name>       Build subdirectory name (default: linux_amd64)
#   --libs=<lib1>,<lib2>,...    Libraries to merge (manual mode only)

set -e

# Parse arguments
OUTPUT_LIB=""
BASE_LIB=""
AR_CMD=""
MODE="smart"
PROJECT_ROOT=""
BUILD_SUBDIR="linux_amd64"
MANUAL_LIBS=()

while [[ $# -gt 0 ]]; do
    case $1 in
        --mode=*)
            MODE="${1#*=}"
            shift
            ;;
        --project-root=*)
            PROJECT_ROOT="${1#*=}"
            shift
            ;;
        --build-subdir=*)
            BUILD_SUBDIR="${1#*=}"
            shift
            ;;
        --libs=*)
            IFS=',' read -ra MANUAL_LIBS <<< "${1#*=}"
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
    echo "Usage: merge_libs.sh <output_lib> <base_lib> <ar_command> [options]"
    exit 1
fi

# Parse .gitmodules and extract submodule names
parse_gitmodules() {
    local gitmodules_file="$1"
    local -n result=$2

    if [ ! -f "$gitmodules_file" ]; then
        return
    fi

    while IFS= read -r line; do
        if [[ "$line" =~ path[[:space:]]*=[[:space:]]*(.+) ]]; then
            submodule_path="${BASH_REMATCH[1]}"
            submodule_path=$(echo "$submodule_path" | xargs)
            submodule_name=$(basename "$submodule_path")
            result+=("$submodule_name")
        fi
    done < "$gitmodules_file"
}

declare -a libs_to_merge

case $MODE in
    manual)
        # Manual mode: use explicitly provided libraries
        if [ ${#MANUAL_LIBS[@]} -eq 0 ]; then
            echo "Error: --libs required for manual mode"
            exit 1
        fi
        libs_to_merge=("${MANUAL_LIBS[@]}")
        echo "=== Manual merge mode ==="
        ;;

    auto)
        # Auto mode: include all libraries from .gitmodules
        if [ -z "$PROJECT_ROOT" ]; then
            echo "Error: --project-root required for auto mode"
            exit 1
        fi

        GITMODULES="${PROJECT_ROOT}/.gitmodules"
        if [ ! -f "$GITMODULES" ]; then
            echo "Error: .gitmodules not found at $GITMODULES"
            exit 1
        fi

        declare -a modules
        parse_gitmodules "$GITMODULES" modules

        echo "=== Auto merge mode ==="
        echo "Found modules: ${modules[@]}"

        for mod in "${modules[@]}"; do
            lib_file="${PROJECT_ROOT}/modules/${mod}/build/${BUILD_SUBDIR}/libfinkit_${mod}_static.a"
            if [ -f "$lib_file" ]; then
                libs_to_merge+=("$lib_file")
            else
                echo "Warning: Library not found: $lib_file"
            fi
        done
        ;;

    smart)
        # Smart mode: intelligent deduplication based on dependency graph
        if [ -z "$PROJECT_ROOT" ]; then
            echo "Error: --project-root required for smart mode"
            exit 1
        fi

        GITMODULES="${PROJECT_ROOT}/.gitmodules"
        if [ ! -f "$GITMODULES" ]; then
            echo "Error: .gitmodules not found at $GITMODULES"
            exit 1
        fi

        declare -a current_modules
        parse_gitmodules "$GITMODULES" current_modules

        echo "=== Smart merge mode (with deduplication) ==="
        echo "Project modules: ${current_modules[@]}"

        # Build dependency graph
        declare -A module_deps

        for mod in "${current_modules[@]}"; do
            submodule_gitmodules="${PROJECT_ROOT}/modules/${mod}/.gitmodules"
            if [ -f "$submodule_gitmodules" ]; then
                declare -a sub_deps
                parse_gitmodules "$submodule_gitmodules" sub_deps

                if [ ${#sub_deps[@]} -gt 0 ]; then
                    echo "  $mod depends on: ${sub_deps[@]}"
                    module_deps["$mod"]="${sub_deps[@]}"
                fi
            fi
        done

        # Determine which modules to include (exclude transitive dependencies)
        declare -a modules_to_include

        for mod in "${current_modules[@]}"; do
            is_dependency=0

            # Check if this module is a dependency of any other module
            for other_mod in "${current_modules[@]}"; do
                if [ "$mod" != "$other_mod" ] && [ -n "${module_deps[$other_mod]}" ]; then
                    if echo "${module_deps[$other_mod]}" | grep -q "\b$mod\b"; then
                        is_dependency=1
                        echo "  Skipping $mod (included in $other_mod)"
                        break
                    fi
                fi
            done

            if [ $is_dependency -eq 0 ]; then
                modules_to_include+=("$mod")
            fi
        done

        echo "Modules after deduplication: ${modules_to_include[@]}"

        for mod in "${modules_to_include[@]}"; do
            lib_file="${PROJECT_ROOT}/modules/${mod}/build/${BUILD_SUBDIR}/libfinkit_${mod}_static.a"
            if [ -f "$lib_file" ]; then
                libs_to_merge+=("$lib_file")
            else
                echo "Warning: Library not found: $lib_file"
            fi
        done
        ;;

    *)
        echo "Error: Invalid mode '$MODE'. Use manual, auto, or smart."
        exit 1
        ;;
esac

# Create MRI script
MRI_SCRIPT=$(mktemp)
trap "rm -f $MRI_SCRIPT" EXIT

echo "CREATE $OUTPUT_LIB" > "$MRI_SCRIPT"
echo "ADDLIB $BASE_LIB" >> "$MRI_SCRIPT"

for lib in "${libs_to_merge[@]}"; do
    echo "ADDLIB $lib" >> "$MRI_SCRIPT"
done

echo "SAVE" >> "$MRI_SCRIPT"
echo "END" >> "$MRI_SCRIPT"

echo ""
echo "=== Final merge order ==="
echo "1. Base: $(basename $BASE_LIB)"
i=2
for lib in "${libs_to_merge[@]}"; do
    echo "$i. $(basename $lib)"
    ((i++))
done

# Execute based on tool type
if [[ "$AR_CMD" == *"lib.exe"* ]]; then
    # Windows: lib.exe
    echo ""
    echo "Using Windows lib.exe..."
    "$AR_CMD" /OUT:"$OUTPUT_LIB" "$BASE_LIB" "${libs_to_merge[@]}"

elif [[ "$AR_CMD" == *"libtool"* ]]; then
    # macOS: libtool
    echo ""
    echo "Using macOS libtool..."
    "$AR_CMD" -static -o "$OUTPUT_LIB" "$BASE_LIB" "${libs_to_merge[@]}" 2>&1 | grep -v "warning duplicate member name" || true

else
    # Linux/Unix: ar with MRI script
    echo ""
    echo "Using ar with MRI script..."
    "$AR_CMD" -M < "$MRI_SCRIPT"
fi

object_count=$(ar t "$OUTPUT_LIB" 2>/dev/null | wc -l || echo "N/A")
echo ""
echo "✓ Merged library created with $object_count objects"
