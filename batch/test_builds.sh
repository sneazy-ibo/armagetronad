#!/bin/bash
# batch/test_builds.sh - Canonical test build script for AI use
# 
# This script builds and tests Armagetron Advanced with multiple configurations,
# making it easy for AI agents and developers to verify changes across different
# build setups.
#
# Usage:
#   ./batch/test_builds.sh [config1] [config2] ... [configN]
#   ./batch/test_builds.sh all
#   TEST_ONLY=1 ./batch/test_builds.sh debug    # Skip build, just test existing build
#   FORCE_RECONFIGURE=1 ./batch/test_builds.sh   # Force re-run of configure
#   VERBOSE=1 ./batch/test_builds.sh            # Show full build output
#
# Available configurations (use 'list' or 'help' to see):
#   default     - Standard client build
#   dedicated   - Dedicated server (no rendering)
#   debug       - Debug level 3 (debug symbols, no optimization)
#   debug2      - Debug level 2
#   debug5      - Debug level 5 (maximum debugging, MEM_DEB)
#   strict      - Code level 3 (strictest warnings)
#   minimal     - Minimal dependencies (no music, curl, auth, krawall)
#   client      - Explicit client build (no server)
#   server      - Dedicated server build
#   master      - Master server build
#   clean       - Clean all test build directories
#
# Exit codes:
#   0 - All configurations passed
#   1 - Command line error
#   >1 - Number of failed configurations
#

set -e

# Ensure we're in the root directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/.."
ROOT="$(pwd)"

# Define configurations: name:configure_flags
CONFIGURATIONS=(
    "default:"
    "dedicated:--enable-dedicated"
    "debug:DEBUGLEVEL=3"
    "debug2:DEBUGLEVEL=2"
    "debug5:DEBUGLEVEL=5"
    "strict:CODELEVEL=3"
    "minimal:--disable-music --disable-curl --disable-authentication --disable-krawall --disable-respawn --disable-memmanager"
    "client:--disable-dedicated --enable-glout"
    "server:--enable-dedicated --disable-glout"
    "master:--enable-master --enable-dedicated --disable-glout"
)

# Common configure flags for all test builds
COMMON_FLAGS="--prefix=/tmp/armagetronad_test --disable-sysinstall --disable-desktop --disable-etc --disable-useradd"

# Parse arguments
if [ $# -eq 0 ] || [ "$1" = "all" ]; then
    SELECTED_CONFIGS=("${CONFIGURATIONS[@]}")
elif [ "$1" = "help" ] || [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
    echo "Usage: $0 [config1|config2|...|all|list|clean|help]"
    echo ""
    echo "Builds and tests Armagetron Advanced with multiple configurations."
    echo ""
    echo "Available configurations:"
    for config in "${CONFIGURATIONS[@]}"; do
        name="${config%%:*}"
        flags="${config#*:}"
        printf "  %-12s %s\n" "$name" "$flags"
    done
    echo ""
    echo "Special targets:"
    echo "  all       - Run all configurations"
    echo "  list      - List available configurations"
    echo "  clean     - Remove all test build directories"
    echo "  help      - Show this help message"
    echo ""
    echo "Environment variables:"
    echo "  TEST_ONLY=1        - Skip building, only run tests on existing builds"
    echo "  FORCE_RECONFIGURE=1 - Force re-run of configure step"
    echo "  VERBOSE=1          - Show full build output (not just summary)"
    echo "  JOBS=N             - Number of parallel jobs (default: auto)"
    exit 0
elif [ "$1" = "list" ]; then
    for config in "${CONFIGURATIONS[@]}"; do
        echo "${config%%:*}"
    done
    exit 0
elif [ "$1" = "clean" ]; then
    for BUILD_DIR in "${ROOT}/build/test_*"; do
        if [ -d "$BUILD_DIR" ]; then
            echo "Removing $BUILD_DIR..."
            rm -rf "$BUILD_DIR"
        fi
    done
    exit 0
else
    SELECTED_CONFIGS=()
    for arg in "$@"; do
        found=false
        for config in "${CONFIGURATIONS[@]}"; do
            if [ "${config%%:*}" = "$arg" ]; then
                SELECTED_CONFIGS+=("$config")
                found=true
                break
            fi
        done
        if [ "$found" = false ]; then
            echo "Error: Unknown configuration '$arg'"
            echo "Use '$0 list' to see available configurations or '$0 help' for full usage."
            exit 1
        fi
    done
fi

# Default number of parallel jobs
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 1)}"

# Bootstrap if needed
if [ ! -x configure ] && [ ! -f configure ]; then
    echo "Bootstrapping autotools..."
    ./bootstrap.sh || {
        echo "Error: bootstrap.sh failed. Make sure autotools are installed."
        exit 1
    }
fi

# Counters
FAILURES=0
TOTAL=0
FAILED_CONFIGS=()

# Process each configuration
for config in "${SELECTED_CONFIGS[@]}"; do
    NAME="${config%%:*}"
    SPECIFIC_FLAGS="${config#*:}"

    BUILD_DIR="$ROOT/build/test_${NAME}"
    
    echo ""
    echo "============================================================"
    echo "Configuration: $NAME"
    if [ -n "$SPECIFIC_FLAGS" ]; then
        echo "Specific flags: $SPECIFIC_FLAGS"
    fi
    echo "Build directory: $BUILD_DIR"
    echo "============================================================"

    TOTAL=$((TOTAL + 1))

    # Create build directory
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    # Configure
    if [ ! -f Makefile ] || [ "$FORCE_RECONFIGURE" = "1" ]; then
        echo "[1/3] Configuring..."
        if [ "$VERBOSE" = "1" ]; then
            eval "../../configure $SPECIFIC_FLAGS $COMMON_FLAGS"
        else
            eval "../../configure $SPECIFIC_FLAGS $COMMON_FLAGS > /tmp/configure_${NAME}.log 2>&1" || {
                echo "Configure FAILED for $NAME"
                echo "Log:"
                cat /tmp/configure_${NAME}.log
                FAILURES=$((FAILURES + 1))
                FAILED_CONFIGS+=("$NAME")
                cd "$ROOT"
                continue
            }
        fi
    fi

    # Build (skip if TEST_ONLY)
    if [ "$TEST_ONLY" != "1" ]; then
        echo "[2/3] Building..."
        if [ "$VERBOSE" = "1" ]; then
            make -j"$JOBS" || {
                echo "Build FAILED for $NAME"
                FAILURES=$((FAILURES + 1))
                FAILED_CONFIGS+=("$NAME")
                cd "$ROOT"
                continue
            }
        else
            make -j"$JOBS" > /tmp/build_${NAME}.log 2>&1 || {
                echo "Build FAILED for $NAME"
                echo "Log tail:"
                tail -20 /tmp/build_${NAME}.log
                FAILURES=$((FAILURES + 1))
                FAILED_CONFIGS+=("$NAME")
                cd "$ROOT"
                continue
            }
        fi
    fi

    # Run tests
    echo "[3/3] Testing..."
    TEST_PASSED=false
    
    # Try make check first
    if ! make check > /tmp/test_${NAME}.log 2>&1; then
        TEST_PASSED=false
    # Try running unit_tests directly
    elif [ -x ./src/unit_tests ] && ./src/unit_tests -ni > /tmp/test_${NAME}.log 2>&1; then
        TEST_PASSED=true
    # Try from build directory
    elif [ -x src/unit_tests ] && src/unit_tests -ni > /tmp/test_${NAME}.log 2>&1; then
        TEST_PASSED=true
    fi
    
    if [ "$TEST_PASSED" = true ]; then
        if [ "$VERBOSE" = "1" ]; then
            cat /tmp/test_${NAME}.log
        fi
        echo "✓ All tests PASSED for $NAME"
    else
        echo "✗ Tests FAILED for $NAME"
        echo "Test log:"
        cat /tmp/test_${NAME}.log
        FAILURES=$((FAILURES + 1))
        FAILED_CONFIGS+=("$NAME")
    fi

    cd "$ROOT"
done

# Print summary
echo ""
echo "============================================================"
echo "Test Build Summary"
echo "============================================================"
echo "Total configurations: $TOTAL"
echo "Passed: $((TOTAL - FAILURES))"
echo "Failed: $FAILURES"

if [ $FAILURES -gt 0 ]; then
    echo ""
    echo "Failed configurations:"
    for failed in "${FAILED_CONFIGS[@]}"; do
        echo "  - $failed"
    done
    echo ""
    echo "To see detailed logs, run with VERBOSE=1"
    echo "To keep build directories for inspection, run with KEEP=1"
fi

echo "============================================================"

exit $FAILURES
