/*
 * server_core_startup.h - Transport-neutral startup for griz-server.
 *
 * Factored out of viewer.c::process_server_mode_stdio() so the RPC
 * transport (Src/server_rpc.c, phase 2) can share the same
 * Analysis / Session / OSMesa / history-file initialization without
 * duplicating the zero-env setup. No I/O loop lives here.
 */

#ifndef SERVER_CORE_STARTUP_H
#define SERVER_CORE_STARTUP_H

#ifdef GRIZ_SERVER_BUILD

#include "viewer.h"

/* Initialize a server session:
 *   - zero the Environ, allocate Analysis + Session
 *   - set serial_batch_mode = TRUE
 *   - optional explicit viewport size
 *   - open_analysis() on db_path
 *   - bring up the OSMesa context via OffscreenContext()
 *   - init_mesh_window() and drive one update_display() frame
 *   - allocate a per-process history file
 *
 * On success returns 0 and writes the new Analysis* to *out_analy.
 * The caller owns the returned pointer (but the per-process shutdown
 * path currently lets the OS reap it; see §7.3 of
 * planning/ui-design/03-server.md).
 *
 * On failure returns non-zero; an error message has already been
 * written to stderr.
 */
int server_core_startup( Analysis **out_analy,
                         const char *db_path,
                         int width, int height );

/* Clean up the per-process history file allocated in startup. Safe to
 * call more than once. */
void server_core_history_cleanup( Analysis *analy );

#endif /* GRIZ_SERVER_BUILD */

#endif /* SERVER_CORE_STARTUP_H */
