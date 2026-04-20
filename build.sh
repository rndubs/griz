#!/usr/bin/env bash
# Build Griz on LLNL TOSS hosts.
#
# Usage:
#   ./build.sh                          # build batchopt + serveropt (default)
#   ./build.sh batch                    # build only batchopt
#   ./build.sh server                   # build only serveropt (griz-server)
#   ./build.sh all                      # alias for default
#   ./build.sh [batch|server|all] \
#              -- [configure-args...]   # override configure flags
#
# Configure defaults to an empty argument list — libjpeg and libpng are
# auto-detected on modern TOSS hosts, so both `outjpeg`/`outpng` and the
# griz-server binary frame encoder end up enabled. Pass
# `./build.sh ... -- --enable-nojpeg --enable-nopng` on a host that
# lacks libjpeg/libpng to compile them out (batch path still links, but
# the inline-screenshot RPC endpoint will fail on kind=0x02 emission).
#
# Output binaries:
#   Src/GRIZ4-*/bin_batch_opt/griz4s.linux_opt_batch   (batch)
#   Src/GRIZ4-*/bin_server_opt/griz-server             (server)
set -euo pipefail

cd "$(dirname "$0")/Src"

TARGET="all"
if [ $# -gt 0 ] && [[ "$1" != -* ]] && [[ "$1" != "--" ]]; then
    case "$1" in
        batch|server|all)
            TARGET="$1"
            shift
            ;;
    esac
fi

# Consume optional `--` separator between target and configure args.
if [ $# -gt 0 ] && [ "$1" = "--" ]; then
    shift
fi

CONFIG_ARGS=("$@")

./configure "${CONFIG_ARGS[@]}"

BUILD_DIR=$(ls -d GRIZ4-* | head -1)
cd "$BUILD_DIR"

case "$TARGET" in
    batch)
        gmake batchopt
        ;;
    server)
        gmake serveropt
        ;;
    all)
        gmake batchopt
        gmake serveropt
        ;;
esac

echo
case "$TARGET" in
    batch)
        echo "Built: $(pwd)/bin_batch_opt/griz4s.linux_opt_batch"
        ;;
    server)
        echo "Built: $(pwd)/bin_server_opt/griz-server"
        ;;
    all)
        echo "Built: $(pwd)/bin_batch_opt/griz4s.linux_opt_batch"
        echo "Built: $(pwd)/bin_server_opt/griz-server"
        ;;
esac
