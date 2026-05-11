/*
 * server_stdio.c - Stdio transport dispatch loop for griz-server.
 *
 * Thin wrapper around server_core_dispatch_line() that reads
 * newline-delimited JSON (or raw) requests from stdin and relies on the
 * default stdout emitter for responses/events.
 *
 * Phase 2 status (planning/ui-design/02-protocol.md):
 *   - JSON request envelope: supported via server_parse_request().
 *   - JSON response envelope: server_emit_response() per command.
 *   - Handshake (ready/hello/hello_ack): supported via server_try_hello().
 *   - Output capture: fd-level, via server_capture_begin/end().
 *   - Transport-neutral dispatcher shared with server_rpc.c.
 *
 * "quit", "exit", "end", or EOF on stdin ends the loop.
 */

#ifdef GRIZ_SERVER_BUILD

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "viewer.h"
#include "server_core.h"
#include "server_core_startup.h"

#define GRIZ_SERVER_STDIO_LINE 4096

int
process_server_mode_stdio( const char *db_path, int width, int height )
{
    Analysis *analy = NULL;
    char line[GRIZ_SERVER_STDIO_LINE];
    int rc;

    /* Reset the emitter to its built-in stdout default in case a
     * previous transport session left an RPC emitter installed (matters
     * only in pathological test harnesses that reuse the same process). */
    server_set_line_emitter( NULL, NULL );

    rc = server_core_startup( &analy, db_path, width, height );
    if ( rc != 0 )
        return rc;

    server_emit_ready();

    while ( fgets( line, sizeof( line ), stdin ) != NULL )
    {
        size_t len = strlen( line );

        while ( len > 0
                && ( line[len - 1] == '\n' || line[len - 1] == '\r' ) )
            line[--len] = '\0';

        if ( server_core_dispatch_line( line, analy ) == 1 )
            break;
    }

    server_core_history_cleanup( analy );

    /* Phase 1 deliberately leaves OSMesa and analysis teardown to the
     * operating system: the analogous batch cleanup path (free(offscreen)
     * + OSMesaDestroyContext + close_analysis) double-frees on our code
     * path because write_image_file() and friends have already released
     * the render buffer. The batch binary never notices because it
     * reaches quit(0)->exit() before glibc audits the heap. Phase 2's
     * output-capture work will rewire this so explicit teardown is
     * safe. */

    return 0;
}

#endif /* GRIZ_SERVER_BUILD */
