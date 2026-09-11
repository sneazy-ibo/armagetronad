#!/usr/bin/env bash
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
# Available configurations (use 'list' or 'help' to see more):
#   client      - Explicit client build (no server)
#   server      - Dedicated server build
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
DEBUG_CONFIGURATIONS=(
    "client_debug:DEBUGLEVEL=3 --disable-dedicated --enable-glout"
    "server_debug:DEBUGLEVEL=3 --enable-master --enable-dedicated --disable-glout"
)

CONFIGURATIONS=(
    "client:--disable-dedicated --enable-glout"
    "server:--enable-master --enable-dedicated --disable-glout"
    "${DEBUG_CONFIGURATIONS[@]}"
    "minimal:--disable-music --disable-authentication --disable-krawall --disable-respawn --disable-memmanager"
)

# -Wno-error=deprecated-declarations currently required because libxml deprecated some things
CXXFLAGS_COMMON='-fmessage-length=0 -D__OPTIMIZE__=1 -Wno-error=deprecated-declarations'

# variations of code strictness flags, the goal is to move down the list
#PEDANTIC_FLAGS=''
PEDANTIC_FLAGS="CODELEVEL=2 CXXFLAGS=\"-Werror ${CXXFLAGS_COMMON}\""
# PEDANTIC_FLAGS='CODELEVEL=3 CXXFLAGS=\"-Werror ${CXXFLAGS_COMMON}\""
# PEDANTIC_FLAGS='CODELEVEL=4 CXXFLAGS=\"-Werror ${CXXFLAGS_COMMON}\""

#echo ${PEDANTIC_FLAGS}

# different compilers -> different directories; store for later
CXX_KEY=""
if [ ! -z "$CXX" ] && [ ! "$CXX" = "c++" ]; then 
    CXX_KEY=_${CXX};
fi

# Common configure flags for all test builds
COMMON_FLAGS="${PEDANTIC_FLAGS} --prefix=/tmp/armagetronad_test --disable-sysinstall --disable-desktop --disable-etc --disable-useradd --enable-curl"

# Parse arguments
if [ $# -eq 0 ] || [ "$1" = "all" ]; then
    SELECTED_CONFIGS=("${CONFIGURATIONS[@]}")
elif [ "$1" = "help" ] || [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
    echo "Usage: $0 [config1|config2|...|debug|all|full|list|clean|help]"
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
    echo "  debug     - Builds default server_debug and client_debug for TDD and debugging"
    echo "  all       - Run all configurations"
    echo "  full      - Build all configurations in all available compilers (clang, gcc and c++)"
    echo "  list      - List available configurations"
    echo "  clean     - Remove all test build directories"
    echo "  help      - Show this help message"
    echo ""
    echo "Environment variables:"
    echo "  TEST_ONLY=1        - Skip building, only run tests on existing builds"
    echo "  BUILD_ONLY=1       - Skip testing, only build"
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
    for BUILD_DIR in ${ROOT}/build/test_*; do
        if [ -d "$BUILD_DIR" ]; then
            echo "Removing $BUILD_DIR..."
            rm -rf "$BUILD_DIR"
        fi
    done
    exit 0
elif [ "$1" = "full" ]; then
    # set -x
    # determine default compiler
    DEFAULT_CXX=${CXX:-c++}
    # we compare compilers by their version output
    DEFAULT_V=`$DEFAULT_CXX -v 2>&1`
    # identify possible compilers
    for COMPILER in g++ clang c++ `ls /usr/bin/g++-* /usr/bin/clang++-* 2>/dev/null | sed -e s,/usr/bin/,,g`; do
        # see if they differ from the default; if yes, build with them
        COMPILER_V=`$COMPILER -v 2>&1` || continue
        if [ "$DEFAULT_V" = "$COMPILER_V" ]; then continue; fi
        echo "Building with $COMPILER..."
        CXX=$COMPILER ./batch/test_builds.sh all || exit $?
    done

    echo "Building with default..."
    ./batch/test_builds.sh all || exit $?
    exit 0
elif [ "$1" = "debug" ]; then
    # two configurations, no special directory tag so tools know where to find them
    SELECTED_CONFIGS=("${DEBUG_CONFIGURATIONS[@]}")
    CXX_KEY=""
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

    # Build key: if this changes, we need to rebuild
    # flags should be self explanatory
    # the root directory is in there to force rebuild on container/host switches
    BUILD_KEY="$SPECIFIC_FLAGS $COMMON_FLAGS $ROOT"

    WORKSPACE_KEY=""
    if [[ $ROOT == /work* ]]; then
        # looks like we are in a devcontainer
        WORKSPACE_KEY="_devcnt"
    fi

    BUILD_DIR="$ROOT/build/test_${NAME}${WORKSPACE_KEY}${CXX_KEY}"
    
    echo ""
    echo "============================================================"
    echo "Configuration: $NAME"
    if [ -n "$SPECIFIC_FLAGS" ]; then
        echo "Specific flags: $SPECIFIC_FLAGS"
    fi
    echo "Build directory: $BUILD_DIR"
    echo "============================================================"

    TOTAL=$((TOTAL + 1))

    STEPS=2
    if [ "$BUILD_ONLY" != "1" ]; then
        STEPS=3
    fi

    # Create build directory
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR" || continue

    # Clear out directory on relevant changes to build configuration
    BUILD_KEY_OLD=`cat build_key 2> /dev/null || true`
    if [ ! "$BUILD_KEY_OLD" = "$BUILD_KEY" ]; then
        #echo BUILD_KEY    =${BUILD_KEY}
        #echo BUILD_KEY_OLD=${BUILD_KEY_OLD}
        if [ -f Makefile ]; then
            echo "[0/$STEPS] Configuration changed, cleaning..."
        fi
        rm -rf *
        echo > build_key "$BUILD_KEY"
    fi

    # Configure
    if [ ! -f Makefile ] || [ "$FORCE_RECONFIGURE" = "1" ]; then
        echo "[1/$STEPS] Configuring..."
        if [ "$VERBOSE" = "1" ]; then
            echo "../../configure $SPECIFIC_FLAGS $COMMON_FLAGS"
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
        echo "[2/$STEPS] Building..."
        if [ "$VERBOSE" = "1" ]; then
            make -k -j"$JOBS" debug || {
                echo "Build FAILED for $NAME"
                FAILURES=$((FAILURES + 1))
                FAILED_CONFIGS+=("$NAME")
                cd "$ROOT"
                continue
            }
        else
            make -k -j"$JOBS" debug > /dev/null 2>&1 || {
                echo "Build FAILED for $NAME"
                echo "Rerun with output:"
                make -k -j"$JOBS" debug || true
                FAILURES=$((FAILURES + 1))
                FAILED_CONFIGS+=("$NAME")
                cd "$ROOT"
                continue
            }
        fi
    fi

    if [ "$BUILD_ONLY" != "1" ]; then
        # Run tests
        echo "[3/3] Testing..."
        TEST_PASSED=false
        
        # Try make check first
        if ! make check > /tmp/test_${NAME}.log 2>&1; then
            TEST_PASSED=false
        # Try running unit_tests directly
        elif [ -x ./src/unit_tests ] && ./src/unit_tests -ni -o=/tmp/test_${NAME}.log; then
            TEST_PASSED=true
        # Try from build directory
        elif [ -x src/unit_tests ] && src/unit_tests -ni -o=/tmp/test_${NAME}.log; then
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
fi

echo "============================================================"

exit $FAILURES
