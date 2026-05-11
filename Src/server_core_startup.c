/*
 * server_core_startup.c - Transport-neutral session initialization for
 * griz-server.
 *
 * Extracted verbatim (minus the stdio dispatch loop) from
 * viewer.c::process_server_mode_stdio() as step 1 of the UI server
 * refactor (planning/ui-design/03-server.md §9). No behavior change.
 */

#ifdef GRIZ_SERVER_BUILD

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "viewer.h"
#include "mesh.h"
#include "misc.h"
#include "server_core_startup.h"

extern Bool_type serial_batch_mode;
extern Session *session;
extern Analysis *analy_ptr;
extern Environ env;

extern int  get_window_width(  void );
extern int  get_window_height( void );
extern void init_griz_session( Session *s );
extern int  OffscreenContext( void *bfp, int width, int height, int ID );
extern Bool_type open_analysis( char *fname, Analysis *analy,
                                Bool_type reload, Bool_type verify_only );

int
server_core_startup( Analysis **out_analy,
                     const char *db_path,
                     int width, int height )
{
    Analysis *analy;
    int rc;

    if ( out_analy == NULL )
        return 1;
    *out_analy = NULL;

    if ( db_path == NULL || db_path[0] == '\0' )
    {
        fprintf( stderr, "griz-server: database path is required\n" );
        return 1;
    }

    memset( &env, 0, sizeof( Environ ) );

    analy   = NEW( Analysis, "Analysis struct" );
    session = NEW( Session,  "Session struct"  );

    env.win32                           = FALSE;
    env.history_input_active            = FALSE;
    env.animate_active                  = FALSE;
    env.animate_reverse                 = FALSE;
    env.show_dialog                     = FALSE;
    env.foreground                      = TRUE;
    env.quiet_mode                      = TRUE;
    env.model_history_logging           = FALSE;
    env.ti_enable                       = TRUE;
    env.griz_id                         = 0;
    env.bname                           = NULL;
    env.checkresults                    = FALSE;
    env.window_size_set_on_command_line = ( width > 0 && height > 0 );

    strncpy( env.plotfile_name, db_path, MAXPATHLENGTH - 1 );
    env.plotfile_name[MAXPATHLENGTH - 1] = '\0';

    /* Server mode is a headless variant of serial batch: reuse the
     * existing flag so code paths that gate on "no X11 available"
     * continue to behave correctly. */
    serial_batch_mode = TRUE;

    if ( width > 0 && height > 0 )
        set_window_size( width, height );

    init_griz_session( session );
    analy_ptr = analy;

    if ( !open_analysis( env.plotfile_name, analy, FALSE, FALSE ) )
    {
        fprintf( stderr, "griz-server: failed to open database '%s'\n",
                 db_path );
        return 1;
    }

    check_for_free_nodes( analy );
    env.curr_analy = analy;
    init_plot_colors();

    /* OffscreenContext() allocates its own RGBA buffer and never writes
     * back through the first argument; passing NULL is intentional. */
    rc = OffscreenContext( NULL,
                           get_window_width(),
                           get_window_height(), 0 );
    if ( rc < 0 )
    {
        fprintf( stderr,
                 "griz-server: OSMesa context init failed (rc=%d)\n", rc );
        return 1;
    }

    init_mesh_window( analy );
    analy->update_display( analy );

    env.griz_pid = getppid();

    /* parse_command() will fclose(analy->p_histfile) on the invalid-
     * command path without a NULL check. Give it a real file to
     * close and truncate so command failures don't crash the server. */
    {
        char hist_path[MAXPATHLENGTH];
        snprintf( hist_path, sizeof( hist_path ),
                  "/tmp/griz-server-%d.grizhist", (int) getpid() );
        analy->p_histfile = fopen( hist_path, "at" );
        strncpy( analy->hist_fname, hist_path,
                 sizeof( analy->hist_fname ) - 1 );
        analy->hist_fname[sizeof( analy->hist_fname ) - 1] = '\0';
    }

    *out_analy = analy;
    return 0;
}

void
server_core_history_cleanup( Analysis *analy )
{
    if ( analy == NULL || analy->p_histfile == NULL )
        return;

    {
        char comment[MAXPATHLENGTH + 8];
        snprintf( comment, sizeof( comment ), "rm %s", analy->hist_fname );
        fclose( analy->p_histfile );
        analy->p_histfile = NULL;
        analy->hist_fname[0] = '\0';
        system( comment );
    }
}

#endif /* GRIZ_SERVER_BUILD */
