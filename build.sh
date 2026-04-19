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
# Configure defaults to --enable-nojpeg --enable-nopng if no args given
# after `--`.
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
if [ ${#CONFIG_ARGS[@]} -eq 0 ]; then
    CONFIG_ARGS=(--enable-nojpeg --enable-nopng)
fi

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
