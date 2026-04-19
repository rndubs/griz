/*
 * server_main.c - Entry point for the griz-server binary.
 *
 * Headless, stdio/RPC-driven sibling of the batch Griz binary. Links
 * against the same object set as batch_opt so the analysis, rendering,
 * and command-interpreter infrastructure can be reused, but replaces
 * the interactive/batch main() with a dispatcher that selects a
 * transport-specific loop based on --transport.
 *
 * Phase 1 scope (see planning/MCP.md): stdio transport only. The
 * per-transport setup and main loop live in viewer.c's
 * process_server_mode_stdio(), which accepts plain-text commands on
 * stdin. The RPC transport is reserved for the Qt UI effort.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "viewer.h"

static void
server_usage(void)
{
    fprintf(stderr,
        "Usage: griz-server --transport={stdio|rpc} -i <database>\n"
        "                   [-w <width> <height>] [--port=N]\n");
}

int
main(int argc, char *argv[])
{
    const char *transport = NULL;
    const char *db_path = NULL;
    int width = 0;
    int height = 0;
    int i;

    for (i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--transport=", 12) == 0) {
            transport = argv[i] + 12;
        } else if (strcmp(argv[i], "-i") == 0) {
            if (++i >= argc) {
                fprintf(stderr,
                    "griz-server: -i requires a database path\n");
                server_usage();
                return 1;
            }
            db_path = argv[i];
        } else if (strcmp(argv[i], "-w") == 0) {
            if (i + 2 >= argc) {
                fprintf(stderr,
                    "griz-server: -w requires <width> <height>\n");
                server_usage();
                return 1;
            }
            width  = atoi(argv[i + 1]);
            height = atoi(argv[i + 2]);
            if (width <= 0 || height <= 0) {
                fprintf(stderr,
                    "griz-server: -w width/height must be positive\n");
                return 1;
            }
            i += 2;
        } else if (strncmp(argv[i], "--port=", 7) == 0) {
            /* Ignored for stdio; consumed by rpc transport later. */
            continue;
        } else if (strcmp(argv[i], "-h") == 0
                   || strcmp(argv[i], "--help") == 0) {
            server_usage();
            return 0;
        } else {
            fprintf(stderr,
                "griz-server: unrecognized option '%s'\n", argv[i]);
            server_usage();
            return 1;
        }
    }

    if (transport == NULL) {
        fprintf(stderr, "griz-server: --transport=<stdio|rpc> is required\n");
        server_usage();
        return 1;
    }

    if (db_path == NULL) {
        fprintf(stderr, "griz-server: -i <database> is required\n");
        server_usage();
        return 1;
    }

    if (strcmp(transport, "stdio") == 0) {
        return process_server_mode_stdio(db_path, width, height);
    }

    if (strcmp(transport, "rpc") == 0) {
        fprintf(stderr,
            "griz-server: transport=rpc is not implemented yet\n");
        return 1;
    }

    fprintf(stderr, "griz-server: unknown transport '%s'\n", transport);
    server_usage();
    return 1;
}
