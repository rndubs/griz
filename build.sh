#!/usr/bin/env bash
# Build Griz batchopt on LLNL TOSS hosts.
# Usage: ./build.sh [configure-args...]
# Defaults to --enable-nojpeg --enable-nopng if no args are given.
set -euo pipefail

cd "$(dirname "$0")/Src"

CONFIG_ARGS=("$@")
if [ ${#CONFIG_ARGS[@]} -eq 0 ]; then
    CONFIG_ARGS=(--enable-nojpeg --enable-nopng)
fi

./configure "${CONFIG_ARGS[@]}"

BUILD_DIR=$(ls -d GRIZ4-* | head -1)
cd "$BUILD_DIR"
gmake batchopt

echo
echo "Built: $(pwd)/bin_batch_opt/griz4s.linux_opt_batch"
