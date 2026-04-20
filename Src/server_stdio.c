/*
 * server_stdio.c - Stdio transport dispatch loop for griz-server.
 *
 * Extracted verbatim from viewer.c::process_server_mode_stdio() as
 * step 1 of the UI server refactor (planning/ui-design/03-server.md §9).
 * No behavior change: startup delegates to server_core_startup(), query
 * commands dispatch through server_try_query(), and parse_command() is
 * invoked inside the same begin/capture/end guards.
 *
 * Phase 2 status (planning/mcp/05-protocol.md):
 *   - JSON request envelope: supported via server_parse_request().
 *   - JSON response envelope: server_emit_response() per command.
 *   - Handshake (ready/hello/hello_ack): supported via server_try_hello().
 *   - Output capture: fd-level, via server_capture_begin/end().
 *
 * "quit", "exit", "end", or EOF on stdin ends the loop.
 */

#ifdef GRIZ_SERVER_BUILD

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "viewer.h"
#include "cJSON.h"
#include "server_core.h"
#include "server_core_startup.h"
#include "server_events.h"
#include "server_query.h"

#define GRIZ_SERVER_MAX_LINE 4096

static Bool_type
server_is_terminator( const char *s )
{
    return ( strcmp( s, "quit" ) == 0
          || strcmp( s, "exit" ) == 0
          || strcmp( s, "end"  ) == 0 );
}

int
process_server_mode_stdio( const char *db_path, int width, int height )
{
    Analysis *analy = NULL;
    char line[GRIZ_SERVER_MAX_LINE];
    int rc;

    rc = server_core_startup( &analy, db_path, width, height );
    if ( rc != 0 )
        return rc;

    server_emit_ready();

    while ( fgets( line, sizeof( line ), stdin ) != NULL )
    {
        ServerRequest req;
        char cmd_buf[GRIZ_SERVER_MAX_LINE];
        size_t len = strlen( line );

        while ( len > 0
                && ( line[len - 1] == '\n' || line[len - 1] == '\r' ) )
            line[--len] = '\0';

        if ( line[0] == '\0' || line[0] == '#' )
            continue;

        /* Optional handshake: consume hello frames and loop back. The
         * client may send zero, one, or more hellos; non-hello JSON
         * and raw command lines fall through to request processing. */
        if ( server_try_hello( line ) )
            continue;

        if ( server_parse_request( line, &req ) != 0 )
        {
            /* Malformed JSON — error response already emitted. */
            continue;
        }

        if ( server_is_terminator( req.cmd ) )
        {
            server_emit_response( req.id, 1, "", "" );
            server_request_free( &req );
            break;
        }

        /* Dispatch query commands (q_state, q_view, q_time, ...) directly —
         * they bypass parse_command and emit a response with a
         * populated `data` field. */
        if ( server_try_query( req.id, req.cmd, analy ) )
        {
            server_request_free( &req );
            continue;
        }

        /* parse_command() takes a mutable buffer (it tokenises in place);
         * copy the resolved command so the cJSON-owned string is not
         * disturbed. Truncate on overflow — GRIZ_SERVER_MAX_LINE matches
         * the input line limit so truncation shouldn't happen in practice
         * unless the client sends a pathological JSON request. */
        strncpy( cmd_buf, req.cmd, sizeof( cmd_buf ) - 1 );
        cmd_buf[sizeof( cmd_buf ) - 1] = '\0';

        server_clear_error();
        server_capture_begin();
        notify_state_reset();
        parse_command( cmd_buf, analy );
        {
            char       *out_s = NULL;
            char       *err_s = NULL;
            const char *err_code    = NULL;
            const char *err_message = NULL;
            int         had_error;

            server_capture_end( &out_s, &err_s );

            had_error = server_peek_error( &err_code, &err_message );
            if ( had_error )
            {
                server_emit_error( req.id, err_code, err_message );
            }
            else
            {
                server_emit_response( req.id, 1,
                                      out_s ? out_s : "",
                                      err_s ? err_s : "" );
            }
            free( out_s );
            free( err_s );

            /* MVP: emit a state_changed event after every successful
             * mutating command. Instrumentation in interpret.c will
             * eventually replace this blanket flag with targeted
             * notify_state(key) calls; see planning/ui-design/
             * 03-server.md §5.1. */
            if ( !had_error )
            {
                notify_state_all();
                notify_state_flush( analy );
            }
        }
        server_request_free( &req );
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
