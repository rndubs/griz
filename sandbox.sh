#!/usr/bin/env bash
#
# sandbox.sh - Reproduce a local "syntax-check" environment for the
# griz-server TUs outside the LLNL TOSS build farm.
#
# The production build depends on the Mili I/O library (installed at
# /usr/apps/mdg on LLNL hosts) which we cannot redistribute. Without
# Mili the batch / server binaries cannot link; instead this script
# sets up enough of the system toolchain and vendors stub headers so
# `gcc -fsyntax-only` can validate that the new server TUs compile.
#
# Usage:
#   ./sandbox.sh install        # install system packages (sudo)
#   ./sandbox.sh stubs          # write Mili/Motif header stubs into
#                               # ./.sandbox/stubs (not committed)
#   ./sandbox.sh check          # syntax-check the server TUs
#   ./sandbox.sh all            # install + stubs + check  (default)
#
# No invocation of ./build.sh — that path needs real Mili. Use a TOSS
# host or a shell with /usr/apps/mdg mounted for a full build.
set -euo pipefail

cd "$(dirname "$0")"
ROOT=$(pwd)
STUB_DIR="$ROOT/.sandbox/stubs"

APT_PACKAGES=(
    autoconf
    build-essential
    libjpeg-turbo8-dev
    libosmesa6-dev
    libmotif-dev
    libxt-dev
    libglw1-mesa-dev
    libgl-dev
    libglu1-mesa-dev
    libpng-dev
)

do_install() {
    sudo apt-get update -qq
    sudo apt-get install -y "${APT_PACKAGES[@]}"
}

do_stubs() {
    mkdir -p "$STUB_DIR"

    cat > "$STUB_DIR/mili.h" <<'EOF'
/* sandbox-only stub for Mili. Enough declarations to parse Griz
 * headers; do NOT link against a binary built with this stub. */
#ifndef MILI_STUB_H
#define MILI_STUB_H

#include <stddef.h>
#include <stdint.h>

typedef int        Mili_family;
typedef void      *Famid;

/* Types from Mili helper headers not redistributed with Griz. Real
 * definitions live in /usr/apps/mdg/... on LLNL hosts. For a syntax-
 * only check we just need enough to parse Griz headers. Subrecord is
 * embedded by value in Griz's Subrec_obj, so it needs a complete type;
 * the others can stay as opaque forwards. */
typedef struct Hash_table_     Hash_table;
typedef struct State_variable_ State_variable;
typedef struct Htable_entry_   Htable_entry;

typedef struct Subrecord_ {
    char  *name;
    int    organization;
    int    qty_svars;
    char **svar_names;
    char  *class_name;
    int    superclass;
    int    qty_objects;
    int    qty_blocks;
    int   *mo_blocks;
} Subrecord;

/* Superclass enum values used by Griz (see Src/io_wrap.h). */
#define M_UNIT       0
#define M_NODE       1
#define M_TRUSS      2
#define M_BEAM       3
#define M_TRI        4
#define M_QUAD       5
#define M_TET        6
#define M_PYRAMID    7
#define M_WEDGE      8
#define M_HEX        9
#define M_MAT       10
#define M_MESH      11
#define M_SURFACE   12
#define M_PARTICLE  13

#define M_QTY_SUPERCLASS 16

#define M_STRING    0
#define M_FLOAT     1
#define M_FLOAT4    2
#define M_FLOAT8    3
#define M_INT       4
#define M_INT4      5
#define M_INT8      6

#define M_MAX_ARRAY_DIMS  8
#define M_MAX_NAME_LEN   64
#define M_MAX_STRING_LEN 256

/* Mili query op codes (unused here, defined to satisfy io_wrap.h). */
#define QTY_STATES              0
#define QTY_DIMENSIONS          1
#define QTY_MESHES              2
#define QTY_SREC_FMTS           3
#define QTY_SUBRECS             4
#define QTY_SUBREC_SVARS        5
#define QTY_SVARS               6
#define QTY_NODE_BLKS           7
#define QTY_NODES_IN_BLK        8
#define QTY_CLASS_IN_SCLASS     9
#define QTY_ELEM_CONN_DEFS     10
#define QTY_ELEMS_IN_DEF       11
#define SREC_FMT_ID            12
#define SERIES_SREC_FMTS       13
#define SUBREC_CLASS           14
#define SREC_MESH              15
#define CLASS_SUPERCLASS       16
#define STATE_TIME             17
#define SERIES_TIMES           18
#define MULTIPLE_TIMES         19
#define STATE_OF_TIME          20
#define CLASS_EXISTS           21
#define LIB_VERSION            22
#define STATE_SIZE             23

#endif /* MILI_STUB_H */
EOF

    cat > "$STUB_DIR/gahl.h" <<'EOF'
/* sandbox-only stub for GAHL (Griz Analysis Helper Library). */
#ifndef GAHL_STUB_H
#define GAHL_STUB_H
#include <stddef.h>
#include <stdint.h>
#endif
EOF

    cat > "$STUB_DIR/griz_config.h" <<'EOF'
/* sandbox-only stub for the autoconf-generated griz_config.h.
 * Real file is produced by ./configure inside a GRIZ4 build tree. */
#ifndef GRIZ_CONFIG_STUB_H
#define GRIZ_CONFIG_STUB_H
#define PACKAGE_VERSION "sandbox"
#endif
EOF

    # Griz headers expect GL headers at <GL/gl.h> and <GL/osmesa.h>
    # (system path on TOSS). The vendored copy under Src/ext/Mesa/include
    # drops the GL/ prefix, so the sandbox mirrors the flat files into a
    # GL/ subdir of the stubs tree.
    mkdir -p "$STUB_DIR/GL"
    for h in gl.h glext.h glxext.h osmesa.h; do
        cp "$ROOT/Src/ext/Mesa/include/$h" "$STUB_DIR/GL/$h"
    done

    echo "stubs written: $STUB_DIR"
}

do_check() {
    cd "$ROOT/Src"

    local CFLAGS=(
        -DGRIZ_SERVER_BUILD=1
        -DSERIAL_BATCH=1
        -DNO_X11=1
        -I.
        -I"$STUB_DIR"
        -Iext/cJSON
        -Iext/Mesa/include
        -std=gnu99
        -fsyntax-only
        -Wall
        -Wno-unused-function
        -Wno-unused-variable
    )

    local FILES=(
        server_core.c
        server_core_startup.c
        server_stdio.c
        server_query.c
        server_events.c
        server_main.c
        server_rpc.c
    )

    local failed=0
    for f in "${FILES[@]}"; do
        echo "=== syntax check: $f ==="
        if ! gcc "${CFLAGS[@]}" -c "$f"; then
            failed=1
        fi
    done

    if [ "$failed" -ne 0 ]; then
        echo "sandbox syntax check FAILED" >&2
        return 1
    fi
    echo "sandbox syntax check OK"
}

ACTION="${1:-all}"
case "$ACTION" in
    install) do_install ;;
    stubs)   do_stubs ;;
    check)   do_check ;;
    all)     do_install; do_stubs; do_check ;;
    *)
        echo "usage: $0 [install|stubs|check|all]" >&2
        exit 2
        ;;
esac
