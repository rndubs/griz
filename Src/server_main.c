/*
 * server_main.c - Entry point for the griz-server binary.
 *
 * This is the headless, stdio/RPC-driven sibling of the batch Griz
 * binary. It links against the same object set as batch_opt so it can
 * reuse the existing analysis, rendering, and command interpreter
 * infrastructure, but replaces the interactive/batch main() with a
 * dispatcher that will eventually enter process_server_mode_stdio() or
 * process_server_mode_rpc() based on a --transport flag.
 *
 * Phase 1 scope (see planning/MCP.md): this file currently provides a
 * minimal main() that just reports startup so the build system can
 * produce the griz-server target from batchopt objects. The transport
 * loops and command dispatch are filled in by subsequent Phase 1 items.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    int i;

    for (i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--transport=", 12) == 0) {
            transport = argv[i] + 12;
        } else if (strcmp(argv[i], "-h") == 0
                   || strcmp(argv[i], "--help") == 0) {
            server_usage();
            return 0;
        }
    }

    if (transport == NULL) {
        fprintf(stderr, "griz-server: --transport=<stdio|rpc> is required\n");
        server_usage();
        return 1;
    }

    if (strcmp(transport, "stdio") != 0 && strcmp(transport, "rpc") != 0) {
        fprintf(stderr, "griz-server: unknown transport '%s'\n", transport);
        server_usage();
        return 1;
    }

    fprintf(stderr,
        "griz-server: build target wired up; transport=%s handler not yet implemented\n",
        transport);
    return 0;
}
