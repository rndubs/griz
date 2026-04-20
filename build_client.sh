#!/usr/bin/env bash
# Build the Griz Qt client on LLNL TOSS hosts.
#
# Usage:
#   ./build_client.sh                        # release build (default)
#   ./build_client.sh debug                  # debug build
#   ./build_client.sh [release|debug] \
#                     -- [cmake-args...]     # extra cmake configure args
#
# MVP ships against Qt 5.15 (system-installed on TOSS). Qt 6 is the eventual
# design target — see planning/ui-design/04-client.md §2.2.
#
# Requirements:
#   - cmake >= 3.16 on PATH (module load cmake/3.26.3 works; the TOSS default
#     cmake/3.23.1 is also sufficient).
#   - Qt 5.15 development headers (system package, no module needed).
#   - A C++17 compiler. The currently loaded compiler module is used.
#
# Output: client/build/linux-<type>/src/griz-client
set -euo pipefail

cd "$(dirname "$0")"

CLIENT_DIR="$PWD/client"

TYPE="release"
if [ $# -gt 0 ] && [[ "$1" != "--" ]]; then
    case "$1" in
        release|Release) TYPE="release"; shift ;;
        debug|Debug)     TYPE="debug";   shift ;;
    esac
fi

if [ $# -gt 0 ] && [ "$1" = "--" ]; then
    shift
fi

EXTRA_ARGS=("$@")

case "$TYPE" in
    release) CMAKE_BUILD_TYPE="Release" ;;
    debug)   CMAKE_BUILD_TYPE="Debug"   ;;
esac

BUILD_DIR="$CLIENT_DIR/build/linux-$TYPE"

if ! command -v cmake >/dev/null 2>&1; then
    echo "cmake not found on PATH. Try: module load cmake/3.26.3" >&2
    exit 2
fi

mkdir -p "$BUILD_DIR"

cmake -S "$CLIENT_DIR" -B "$BUILD_DIR" \
      -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" \
      "${EXTRA_ARGS[@]}"

cmake --build "$BUILD_DIR" -j "$(nproc)"

echo
echo "Built: $BUILD_DIR/src/griz-client"
