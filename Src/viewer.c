
/* $Id$ */
/*
 * viewer.c - MDG mesh viewer.
 *
 *      Donald J. Dovey
 *      Lawrence Livermore National Laboratory
 *      Dec 27, 1991
 *
 *
  ************************************************************************
 * Modifications:
 *  I. R. Corey - Dec 22, 2004: Add new function to display free nodes.
 *                See SRC# 292.
 *
 *  I. R. Corey - Jan  5, 2005: Added new option to allow for selecting
 *                a beta version of the code. This option (-beta) is
 *                also parsed by the griz4s shell script.
 *                 See SRC# 292.
 *
 *  I. R. Corey - Jan 24, 2005: Added a new free_node option to disable
 *                mass scaling of the nodes.def
 *
 *  I. R. Corey - Feb 10, 2005: Added a new free_node option to enable
 *                volume disable scaling of the nodes.
 *
 *  I. R. Corey - May 19, 2005: Added new option to allow for selecting
 *                a alpha version of the code. This option (-alpha) is
 *                also parsed by the griz4s shell script.
 *                See SCR #328.
 *
 *  J. K. Durrenberger
 *              - June 24, 2005 commented line 367 to allow the running
 *                of the code in batch mode with no xterm available. When
 *                running as a cronjob or running with ssh with X11
 *                tunneling disabled the software would try to instantiate
 *                a window and exit with an error at this point.
 *
 *  I. R. Corey - October 24, 2006: Add a class selector to all vis/invis
 *                and enable/disable commands.
 *                See SRC#421.
 *
 *  I. R. Corey - May 15, 2007: Added more functionality to the session
 *                read-write function.
 *                See SRC#421.
 *
 *  I. R. Corey - May 15, 2007:	Save/Restore window attributes.
 *                See SRC#458.
 *
 *  I. R. Corey - Aug 09, 2007:	Fixed initialization bug with object
 *                selection arrays (enable/disable).
 *                See SRC#479.
 *
 *  I. R. Corey - Aug 15, 2007:	Added menus for displaying TI results.
 *                See SRC#480
 *
 *  I. R. Corey - Nov 08, 2007:	Add Node/Element labeling.
 *                See SRC#418 (Mili) and 504 (Griz)
 *
 *  I. R. Corey - Apr 18, 2008:	Allow for longer commands when reading
 *                from history file.
 *                See SRC#529
 *
 *  I. R. Corey - June 07, 2008: Force Griz to run in foreground when
 *                running in batch mode so that all I/O is completed
 *                before Griz job returns to script.
 *                See SRC#539
 *
 *  J.K.Durrenberger - June 13, 2008 Added a check of the batch option
 *		  if no -b option was passed to Griz we exit with an an
 *		  error.
 *
 *  I. R. Corey - June 16, 2009: Add exclusion capability for nodes.
 *                See SRC#539
 *
 *  I. R. Corey - June 16, 2009: Add exclusion capability for nodes.
 *                See SRC#539
 *
 *  I. R. Corey - Nov 09, 2009: Added enhancements to better support
 *                running multiple sessions of Griz.
 *                See SRC#638.
 *
 *  I. R. Corey - Dec 28, 2009: Fixed several problems releated to drawing
 *                ML particles.
 *                See SRC#648
 *
 *  I. R. Corey - Dec 28, 2010:
 *                ML particles.
 *                See SRC#648
 *
 *  I. R. Corey - June 28, 2010: Added command line argument to override
 *                the banner filename - useful when making pictures for
 *                presentations.
 *                See SRC#648
 *
 *  I. R. Corey - July 22, 2011: Added check for full file name on input.
 *
 *  I. R. Corey - June 26th, 2012: Added flag to disable echo of commands
 *                in GUI - 'echocmd'. Default value is ON.
 *                See TeamForge#18255
 *
 *  I. R. Corey - July 26th, 2012: Added capability to plot a Modal
 *                database from Diablo.
 *                See TeamForge#18395 & 18396
 *
 *  I. R. Corey - October 4th, 2012: Fixed problem with checking for file
 *                on a load function.
 *
 *  I. R. Corey - February 14th, 2013: Removed code to load history commands
 *                on a reload since the history commands are now written to
 *                a file and the history buffer may contain commands for
 *                multiple reloads.
 *
 *  I. R. Corey - March 18th, 2013: Added support for multiple integration
 *                points per element set.
 *                See TeamForge#18395 & 18396
 *
 ************************************************************************
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "viewer.h"
#include "mesh.h"
#include "misc.h"

#ifdef SERIAL_BATCH
#include <GL/osmesa.h>
OSMesaContext OSMesa_ctx;

static void *offscreen;

Bool_type serial_batch_mode;

static void process_serial_batch_mode( char *batch_input_file_name,
                                       Analysis *analy );
extern void init_app_context_serial_batch( int argc, char *argv[] );
extern int get_window_width();
extern int get_window_height();
extern void init_griz_session( Session * );
int compints(const void *, const void *);

#else
/*
int     batch = 0;
int     bufsize;
void    *Buf;
void    *offscreen;

static  unsigned short Width  = DEFAULT_WIDTH;
static  unsigned short Height = DEFAULT_HEIGHT;

extern  void init_mesh_window( Analysis *analy );
extern  void init_btn_pick_classes();
*/
#endif /* SERIAL_BATCH */
int alphanum_cmp(const void* ,const void*);


Environ env;

Database_type_griz db_type;

Session  *session;
Analysis *analy_ptr;

/******************************************************************
 * This MACRO is used to perform copies between the analysis state
 * and the session buffer.
 ******************************************************************
 */
void process_update_session( int operation, void *ptr, int cnt,
                             int var_id, char *var_name,
                             void *analy_var, void *session_var,
                             int type, int len, Bool_type local_var,
                             short *var_update );

#define QUOTE_ARG(arg) #arg
#define SESSION_VAR( operation, fp, cnt, var_id, var, type, len, local_var ) \
  process_update_session( operation, fp, cnt, var_id, QUOTE_ARG(var), (void *) &analy_ptr->var, \
			  (void *) &session->var, type, len, local_var, session->var_update );

char *session_file_buff[2000];

/*****************************************************************/

static char *non_persist_cmds[] = { "rx",    "ry",     "rz" , "tx", "ty",   "tz",    "anim", "load", "reload", "copyrt", /* 1-10 */
                                    "scale", "scalax", "hist", "h", "exec", "alias", "quit", "animc"
                                  }; /* 11-18 */
static int  num_non_persist_cmds = 18;

char      *history_log[MAXHIST];
int        history_log_index=0;
Bool_type  history_inputCB_cmd;

static char      *model_history_log[MAXHIST]; /* A log of specific commands for the current model */
static int        model_history_log_index=0;
static Bool_type  model_loading_phase=FALSE;

static int label_length = 32;

/* The current version of the code. */
char *griz_version = PACKAGE_VERSION;

/* Required short name for a particle G_UNIT class. */
char *particle_cname = "part";

static void scan_args( int argc, char *argv[], Analysis *analy );
Bool_type open_analysis( char *fname, Analysis *analy, Bool_type reload, Bool_type verify_only );
static void usage( void );

extern void log_variable_scale( float, float, int, float *, float *,
                                float *, float *, int * );

extern void
write_history_text( char *, Bool_type );


#ifndef GRIZ_SERVER_BUILD
/************************************************************
 * TAG( main )
 *
 * Griz main routine.
 *
 * Compiled out when GRIZ_SERVER_BUILD is defined so the griz-server
 * binary can supply its own main() while still linking viewer.c's
 * helper routines.
 */
int
main( int argc, char *argv[] )
{
    Analysis *analy;

    Bool_type status;

    double result[500];
    int i;


#ifdef SERIAL_BATCH
    register int
    rc;

    int osmesa_window_width;
    int osmesa_window_height;
#endif

    /* Clear out the env struct just in case. */
    memset( &env, 0, sizeof( Environ ) );

    /* Open the plotfiles and initialize analysis data structure. */
    analy   = NEW( Analysis, "Analysis struct" );
    session = NEW( Session,  "Session struct"  );

    /* Initialize some environment variables */
    env.win32                = FALSE;
    env.history_input_active = FALSE;
    env.animate_active       = FALSE;
    env.animate_reverse      = FALSE;

    env.show_dialog          = TRUE;
    env.window_size_set_on_command_line = FALSE;

    /* Scan command-line arguments, other initialization. */
    scan_args( argc, argv, analy );

    /* Run in the background by default to free shell window */
    if ( !env.foreground )
    {
        fprintf( stderr, "Main: putting GRIZ in background.\n" );
        switch( fork() )
        {
        case 0:
            break;
        case -1:
            fprintf( stderr, "Fork failed.  Running in foreground.\n" );
            break;
        default:
            exit( 0 );
        }
    }

    /* Initialize the Session Data structure */
    init_griz_session( session ) ;

#ifdef TIME_GRIZ
    manage_timer( 8, 0 );
#endif

#ifdef TIME_OPEN_ANALYSIS
    manage_timer( 1, 0 );
#endif

    env.ti_enable = TRUE;
    analy_ptr = analy;

    if (strlen(env.plotfile_name))
    {
        status = open_analysis( env.plotfile_name, analy, FALSE, FALSE );
        if ( !status )
            exit( 1 );

        check_for_free_nodes( analy );
    }

#ifdef TIME_OPEN_ANALYSIS
    printf( "Open_analysis timing...\n" );
    manage_timer( 1, 1 );
#endif

    env.curr_analy = analy;

    /* Assign these outside of open_analysis() so they have session lifetime. */
    init_plot_colors();

#ifdef SERIAL_BATCH

    if ( serial_batch_mode )
    {
        char comment[512];
        /*
         * perform required GRIZ and serial batch initializations
         * MUST do non-graphic initializations found in "init_mesh_window"
         * to set up v_win, etc.
         */
        if ( env.quiet_mode==FALSE)
        {
            write_start_text();
        }

        /**
         * Commented this out to all run in batch mode when xterm is
         * unavailable.
             *  init_app_context_serial_batch( argc, argv );
         */

        /*
         * establish offscreen rendering context needed by OSMesa
         *
         * NOTE:  window width and height may be set on GRIZ command line
         *        using parameter sequence:  -w <width> <height>
         */

        osmesa_window_width  = get_window_width();
        osmesa_window_height = get_window_height();

        if ( (rc = OffscreenContext( offscreen, osmesa_window_width,
                                     osmesa_window_height, 0 )) < 0 )
        {
            /*
             * GUI's will NOT be available in batch mode, but errors and
             * messages may be written to stdout/err.
             */

            popup_dialog( INFO_POPUP,
                          "Unable to create offscreen rendering context -- rc:  %d\n",
                          rc );
            quit( -1 );
        }
        process_serial_batch_mode( env.input_file_name, env.curr_analy );

        /*
         * Only get to this point via batch input file command(s):  quit, exit,
         * end, or if EOF condition encountered
         */

        free( offscreen );
        OSMesaDestroyContext( OSMesa_ctx );
        if(analy->p_histfile)
        {
            strcpy(comment, "rm ");
            strcat(comment, analy->hist_fname);
            fclose(analy->p_histfile);
            analy->p_histfile = NULL;
            analy->hist_fname[0] = '\0';
            system(comment);
        }
        quit( 0 );
    }

#endif

    /*
     * Initialize variables dealing with vis/invis and enable/disable.
     */
    if (analy->mesh_table)
    {
        MESH( analy ).mat_hide_qty            = 0;

        MESH( analy ).brick_hide_qty          = 0;
        MESH( analy ).brick_disable_qty       = 0;
        MESH( analy ).brick_exclude_qty       = 0;
        MESH( analy ).hide_brick_elem_qty     = 0;
        MESH( analy ).disable_brick_elem_qty  = 0;
        MESH( analy ).brick_result_min        = 0.;
        MESH( analy ).brick_result_max        = 0.;
        MESH( analy ).hide_brick_by_mat       = FALSE;
        MESH( analy ).hide_brick_by_result    = FALSE;

        MESH( analy ).beam_hide_qty          = 0;
        MESH( analy ).beam_disable_qty       = 0;
        MESH( analy ).beam_exclude_qty       = 0;
        MESH( analy ).hide_beam_elem_qty     = 0;
        MESH( analy ).disable_beam_elem_qty  = 0;
        MESH( analy ).beam_result_min        = 0.;
        MESH( analy ).beam_result_max        = 0.;
        MESH( analy ).hide_beam_by_mat       = FALSE;
        MESH( analy ).hide_beam_by_result    = FALSE;

        MESH( analy ).shell_hide_qty         = 0;
        MESH( analy ).shell_disable_qty      = 0;
        MESH( analy ).shell_exclude_qty      = 0;
        MESH( analy ).hide_shell_elem_qty    = 0;
        MESH( analy ).disable_shell_elem_qty = 0;
        MESH( analy ).shell_result_min       = 0.;
        MESH( analy ).shell_result_max       = 0.;
        MESH( analy ).hide_shell_by_mat       = FALSE;
        MESH( analy ).hide_shell_by_result    = FALSE;

        MESH( analy ).truss_hide_qty         = 0;
        MESH( analy ).truss_disable_qty      = 0;
        MESH( analy ).truss_exclude_qty      = 0;
        MESH( analy ).hide_truss_elem_qty    = 0;
        MESH( analy ).disable_truss_elem_qty = 0;
        MESH( analy ).truss_result_min       = 0.;
        MESH( analy ).truss_result_max       = 0.;
        MESH( analy ).hide_truss_by_mat       = FALSE;
        MESH( analy ).hide_truss_by_result    = FALSE;

        MESH( analy ).beam_hide_qty          = 0;
        MESH( analy ).beam_disable_qty       = 0;
        MESH( analy ).beam_exclude_qty       = 0;
        MESH( analy ).hide_beam_elem_qty     = 0;
        MESH( analy ).disable_beam_elem_qty  = 0;
        MESH( analy ).beam_result_min        = 0.;
        MESH( analy ).beam_result_max        = 0.;
        MESH( analy ).hide_beam_by_mat       = FALSE;
        MESH( analy ).hide_beam_by_result    = FALSE;

        MESH( analy ).particle_hide_qty      = 0;
        MESH( analy ).particle_disable_qty   = 0;
        MESH( analy ).hide_particle_by_result= FALSE;
    }

    analy->ei_result_name[0] = '\0';
    analy->hist_paused       = FALSE;

    analy->extreme_result   = FALSE;
    analy->extreme_min      = FALSE;
    analy->extreme_max      = FALSE;

    analy->mm_result_set[0] = FALSE;
    analy->mm_result_set[1] = FALSE;
    analy->mm_time_set[0]   = FALSE;
    analy->mm_time_set[1]   = FALSE;

    analy->th_plotting      = FALSE;
    analy->th_single_col    = FALSE;
    analy->th_append_output = FALSE;

    analy->infoMsgCnt       = 0;
    for ( i=0;
            i<MAXINFOMSG;
            i++ )
        analy->infoMsg[i]=NULL;

    /* Initialize the history log */
    for ( i=0;
            i<MAXHIST;
            i++ )
    {
        history_log[i]       = (char *) NULL;
        model_history_log[i] = (char *) NULL;
    }

    /*
     * Get the process id for this griz session.
     */
    env.griz_pid = getppid();

    /*
     * All other modes (at least for time being -- 03-10-00)
     */
    gui_start( argc, argv, analy );


#ifdef TIME_GRIZ
    manage_timer( 8, 1 );
#endif
}
#endif /* GRIZ_SERVER_BUILD */


/************************************************************
 * TAG( scan_args )
 *
 * Parse the program's command line arguments.
 */
static void
scan_args( int argc, char *argv[], Analysis *analy )
{
    extern Bool_type include_util_panel, include_mtl_panel; /* From gui.c */
    int i;
    int inname_set;
    char *fontlib;
    time_t time_int;
    struct tm *tm_struct;
    struct stat st;
    char *name;

    inname_set = FALSE;
    env.plotfile_name[0] = '\0';

    env.ti_enable = TRUE;
    env.griz_id   = 0;

    env.bname = NULL;
    env.checkresults          = FALSE;
    env.quiet_mode            = FALSE;
    env.model_history_logging = TRUE;

    for ( i = 1; i < argc; i++ )
    {
        /* Save a complete copy of the command line */
        strcat(env.command_line, " ");
        strcat(env.command_line, argv[i]);


        if ( strcmp( argv[i], "-i" ) == 0 )
        {
            /* Olga's very first bug fix */
            /* If i >= argc, then Griz just dumps core */
            /* User's don't like this and so now we check */

            i++;
            if ( i >= argc )
            {
                usage();
                exit( 0 );
            }

            strcat(env.command_line, " ");
            strcat(env.command_line, argv[i]);

            strcpy( env.plotfile_name, argv[i] );
            inname_set = TRUE;
        }
        else if ( strcmp( argv[i], "-s" ) == 0 )
            env.single_buffer = TRUE;
        else if ( strcmp( argv[i], "-v" ) == 0 )
            set_window_size( VIDEO_WIDTH, VIDEO_HEIGHT );
        else if ( strcmp( argv[i], "-V" ) == 0 )
        {
            VersionInfo(FALSE);
            exit( 0 );
        }
        else if ( strcmp( argv[i], "-alpha" ) == 0 )
        {
            env.run_alpha_version = 1.0;
        }
        else if ( strcmp( argv[i], "-beta" ) == 0 )
        {
            env.run_beta_version = 1.0;
        }
        else if ( strcmp( argv[i], "-man" ) == 0 )
        {
            parse_command( "help", analy );
            exit( 0 );
        }
        else if ( strcmp( argv[i], "-w" ) == 0 )
        {
            if ( i + 2 <= argc )
            {
                set_window_size( atoi( argv[i + 1] ), atoi( argv[i + 2] ) );
                env.window_size_set_on_command_line = TRUE;
                i += 2;
            }
            else
            {
                usage();
                exit( 0 );
            }
        }
        else if ( strcmp( argv[i], "-f" ) == 0 )
            env.foreground = TRUE;
        else if ( strcmp( argv[i], "-gid" ) == 0 )
        {
            env.griz_id = atoi( argv[i + 1] );
            if ( env.griz_id<1 )
                env.griz_id = 0;
            if ( env.griz_id>10 )
                env.griz_id = env.griz_id % 10;
            i++;
        }

        else if ( strcmp( argv[i], "-win32" ) == 0 )
            env.win32 = TRUE;

        else if ( strcmp( argv[i], "-bname" ) == 0 )
        {
            env.bname = strdup( argv[i + 1] );
            i++;
        }
        else if ( strcmp( argv[i], "-q" ) == 0 )
        {
            env.quiet_mode = TRUE;
            VersionInfo(TRUE);
        }
        else if ( strcmp( argv[i], "-nohist" ) == 0 )
        {
            env.model_history_logging = FALSE;
        }

#ifdef SERIAL_BATCH
        /*
         * do NOT establish utility panel for batch operations
         */
#else
        else if ( strcmp( argv[i], "-u" ) == 0 )
            include_util_panel = TRUE;
        else if ( strcmp( argv[i], "-m" ) == 0 )
            include_mtl_panel = TRUE;
        else if ( strcmp( argv[i], "-nodialog" ) == 0 )
            env.show_dialog = FALSE;
#endif

#ifdef SERIAL_BATCH
        else if ( strcmp( argv[i], "-b" ) == 0 ||
                  strcmp( argv[i], "-batch" ) == 0)
        {
            i++;

            if ( i >= argc )
            {
                usage();
                exit( 0 );
            }

            if ( NULL != argv[i] )
            {
                strcat(env.command_line, " ");
                strcat(env.command_line, argv[i]);

                strcpy( env.input_file_name, argv[i] );
                serial_batch_mode = TRUE;


                /* If in batch mode - force to run in
                 * foreground so all I/O is completed
                 * before Griz returns.
                 */
                env.foreground = TRUE;
            }
            else
            {
                usage();
                exit( 0 );
            }
        }
#endif
    }
#ifdef SERIAL_BATCH
    if(serial_batch_mode != TRUE)
    {
        fprintf( stderr, "Batch compiled code requires a -b option.\n" );
        usage();
        exit(0);
    }
#endif


    /* If no input file then print usage and exit */
    if ( !inname_set )
    {
        usage();
        exit( 0 );
    }

    /* Make sure font path is set. */
    fontlib = getenv( "HFONTLIB" );

    if ( fontlib == NULL )
    {
#ifndef SERIAL_BATCH
        popup_dialog( WARNING_POPUP, "\tNo HFONTLIB Env Variable founnd. Set HFONTLIB env variable to font file location" );
        popup_dialog( WARNING_POPUP, "\tUsing default HFONTLIB Path = /usr/apps/mdg/archive/grizdir/grizversion/Font" );
#endif

        if(stat("/usr/apps/mdg/archive/grizdir/grizversion/Font",&st) == 0)
            fontlib = strdup( "/usr/apps/mdg/archive/grizdir/grizversion/Font" );
        else
            fontlib = strdup( "." );
    }

    /* Get today's date. */
    time_int = time( NULL );
    env.time_start = time_int; /* Store Griz startup time */
    tm_struct = localtime( &time_int );
    strftime( env.date, 20, "%D", tm_struct );
    strftime( env.time, 20, "%T", tm_struct );

    /* Get user's name. */
    env.user_name[0]='\0';
    name = getenv( "LOGNAME" );
    if ( name!=NULL )
        strcpy( env.user_name, name );

    /* Get hostname */
    env.host[0]='\0';
    name = getenv( "HOST" );
    if ( name!=NULL )
        strcpy( env.host, name );

    /* Get arch */
    env.arch[0]='\0';
    name = getenv( "MACHTYPE" );
    if ( name!=NULL )
        strcpy( env.arch, name );

    /* Get systype */
    env.systype[0]='\0';
    name = getenv( "SYS_TYPE" );
    if ( name!=NULL )
        strcpy( env.systype, name );

    /* Get logging file path */
    env.grizlogdir[0]='\0';
    name = getenv( "GRIZLOGDIR" );
    if ( name!=NULL )
        strcpy( env.grizlogdir, name );

    /* Get logging file name */
    env.grizlogfile[0]='\0';
    name = getenv( "GRIZLOGFILE" );
    if ( name!=NULL )
        strcpy( env.grizlogfile, name );

    /* Get logging file enable flag */
    name = getenv( "LOGFILE" );
    env.enable_logging = FALSE;
    if ( name!=NULL )
        if ( strcmp(name, "yes") == 0 )
            env.enable_logging = TRUE;
}


/************************************************************
 * TAG( load_known_mili_parameters )
 *
 * Check the mili database for any known parameters that Griz tries to use.
 * Load them in and store in the analysis struct.
 */
void
load_known_mili_parameters( Analysis *analy )
{
    int stat;
    Bool_type particles_on;

    /* Check for "particles_on" Mili parameter and set particle_nodes_enabled_flag */
    stat = mc_read_scalar(analy->db_ident, "particles_on", &particles_on);
    // If "particles_on" paramter does not exist, default to off.
    if( stat != OK )
        particles_on = 0;
    analy->particle_nodes_enabled = particles_on;

    /* Check for "job_id" Mili parameter */
    stat = mc_read_string( analy->db_ident, "job_id", analy->job_id );
}


/************************************************************
 * TAG( generate_banned_names_list )
 *
 * Generate list of banned names in Griz.
 * Banned names include existing commands and element class names
 */
void
generate_banned_names_list( Analysis * analy )
{
    int i;
    int qty_banned_names;
    int qty_classes;
    char *class_names[2000];
    int  superclasses[2000];

    //predefined names
    int qty_predefined_names = 16;
    char *predefined_names[] = {"vis","invis","enable","disable","include","exclude","hilite",
                                "select","mat","all","amb","diff","spec","spotdir","spot","exp"};

    /* Get all class names */
    mili_get_class_names( analy, &qty_classes, class_names, superclasses );

    qty_banned_names = qty_classes + qty_predefined_names;

    /* Allocate enough space for all banned names */
    analy->banned_names = malloc(qty_banned_names * sizeof(char*));
    analy->num_banned_names = 0;

    // Copy over predefined names
    for( i = 0 ; i < qty_predefined_names; i++ )
    {
        analy->banned_names[ analy->num_banned_names ] = malloc(100 * sizeof(char));
        strcpy( analy->banned_names[ analy->num_banned_names ], predefined_names[i] );
        analy->num_banned_names++;
    }

    // Copy over class names
    for( i = 0; i < qty_classes; i++ )
    {
        analy->banned_names[ analy->num_banned_names ] = malloc(100 * sizeof(char));
        strcpy( analy->banned_names[ analy->num_banned_names ], class_names[i] );
        analy->num_banned_names++;
    }
}


/************************************************************
 * TAG( open_analysis )
 *
 * Open a plotfile and initialize an analysis.
 */
Bool_type
open_analysis( char *fname, Analysis *analy, Bool_type reload, Bool_type verify_only )
{
    int num_states;
    int i, j, k, l;
    int stat;
    int pos;
    char *first_token, copy_command[512], timestamp[64], timestr[64];
    char hist_fname[256];
    char * datetime = NULL;
    char header[] = "# This is the history file for debugging purposes only\
\n# and is for use by the griz team. Do not modify or alter in any way.\n";
    time_t curtime;
    struct tm *timeinfo=NULL;
    MO_class_data **class_array;
    MO_class_data *p_mocd=NULL, *p_ml_class=NULL, *p_brick_class=NULL,
                   *p_node_geom=NULL;
    Mesh_data *p_md, *p_mesh;
    Visibility_data *p_vd;
    Surface_data *p_surface;
    int class_qty=0, elem_class_qty=0, class_index=0;
    int mat_sz, max_sz, surface_sz, surf_max_sz;
    int surf, facets;
    int max_face_qty, face_qty;
    int cur_mesh_mat_qty;
    int cur_mesh_surf_qty;
    int srec_id;
    int sclass;
    int num_nodes=0;

    int brick_qty=0, shell_qty=0, truss_qty=0, beam_qty=0;
    int particle_qty=0, tet_qty = 0;

    char temp_fname[MAXPATHLENGTH];
    int dir_pos=-1;

    /* TI Variables */
    int  num_entries = 0;
    int  superclass, datatype, datalength, meshid;
    int  state, matid;
    char temp_name[128], class[32];
    char **wildcard_list = NULL;
    char particle_class_name[128];

    Bool_type isMeshvar=FALSE;
    Bool_type isNodal=FALSE;
    int status;

    int *pn_node_list, num_ml_nodes=0;

    /*
     * Don't use popup_fatal() on failure before db is actually open.
     */

    strcpy( temp_fname, fname );
    if ( !is_known_db( fname, &db_type ) )
    {

        /* User may have typed the full filename string with 1 or 2 char extensions */
        /* Try a name with 1 and 2 trailing characters omitted */
        if ( strlen(fname)>=3 )
        {
            temp_fname[strlen(temp_fname)-1]='\0';  /* Omit last character */
            if ( !is_known_db( temp_fname, &db_type ) )
            {
                temp_fname[strlen(temp_fname)-1]='\0'; /* Omit last 2 characters */
            }
        }
    }

    if ( !is_known_db( temp_fname, &db_type ) )
        strcpy( temp_fname, fname );

    /* Initialize I/O functions by database type. */
    if ( !is_known_db( temp_fname, &db_type ) )
    {
        popup_dialog( WARNING_POPUP, "Unable to identify database type: %s.", temp_fname );
        return FALSE;
    }
    else if ( !init_db_io( db_type, analy ) )
    {
        popup_dialog( WARNING_POPUP,
                      "Unable to initialize Griz I/O interface." );
        return FALSE;
    }

    analy->path_name[0] = '\0';
    analy->path_found   = FALSE;
    dir_pos = -1;
    for ( i = 0; i < strlen(temp_fname); i++ )
    {
        if ( temp_fname[i]=='/' )
            dir_pos = i;
    }
    if ( dir_pos>0)
    {
        analy->path_found = TRUE;
        strcpy( analy->path_name, temp_fname );
        analy->path_name[dir_pos+1] = '\0';
    }
    else
    {
        getcwd( analy->path_name, MAXPATHLENGTH );
        strcat( analy->path_name, "/" );
    }

    if ( dir_pos>=0 )
    {
        strcpy( analy->root_name,  &temp_fname[dir_pos+1] );
        strcpy( env.plotfile_name, &temp_fname[dir_pos+1] );
    }
    else
    {
        strcpy( analy->root_name, temp_fname );
        strcpy( env.plotfile_name, temp_fname );
    }
    analy->show_title      = FALSE;
    analy->show_title_path = FALSE;

    /* Enable reading of TI data if requested */
    if ( env.ti_enable )
        mc_ti_enable( analy->db_ident ) ;

    /* Open the database. */
    stat = analy->db_open( temp_fname, &analy->db_ident );

    if ( stat != OK )
    {
        reset_db_io( db_type );
        popup_dialog( WARNING_POPUP, "Unable to initialize database during open." );
        return FALSE;
    }

    if ( verify_only )
        return TRUE;

    stat = analy->db_get_title( analy->db_ident, analy->title );
    if ( stat != OK )
        popup_dialog( INFO_POPUP, "open_analysis() - unable to load title." );

    stat = analy->db_get_dimension( analy->db_ident, &analy->dimension );
    if ( stat != OK )
    {
        popup_fatal( "open_analysis() - unable to get db dimension." );
        return FALSE;
    }
    if(analy->dimension != 2 && analy->dimension !=3)
    {
        popup_fatal("open_analysis() db dimension should be 2 or 3.");
        return FALSE;
    }

    if ( analy->dimension == 2 )
        analy->limit_rotations = TRUE;

    stat = analy->db_get_geom( analy->db_ident, &analy->mesh_table, &analy->mesh_qty );
    if ( stat != OK )
    {
        /*
         * When Griz doesn't have to be started with a db identified,
         * do something friendly to get back to a wait state.
         * Until then...
         */
        return FALSE;
    }

    stat = analy->db_get_st_descriptors( analy, analy->db_ident );
    if ( stat != OK )
        return FALSE;

    stat = analy->db_set_results( analy );
    if ( stat != OK )
        return FALSE;
    
    /* Load any known mili parameters that Griz uses */
    load_known_mili_parameters( analy );

#ifdef TIME_OPEN_ANALYSIS
    printf( "Timing initial state get...\n" );
    manage_timer( 0, 0 );
#endif

    stat = analy->db_get_state( analy, 0, analy->state_p, &analy->state_p, &num_states );
    if ( stat != 0 )
        return FALSE;

#ifdef TIME_OPEN_ANALYSIS
    manage_timer( 0, 1 );
    putc( (int) '\n', stdout );
#endif

    /* set state count in analysis struct */
    analy->state_count = num_states;

    if ( num_states > 0 )
    {
        srec_id = analy->state_p->srec_id;
        analy->cur_mesh_id =
            analy->srec_tree[srec_id].subrecs[0].p_object_class->mesh_id;
    }
    else
        analy->cur_mesh_id = 0;

    if ( analy->dimension == 2 )
        analy->normals_constant = TRUE;
    else
        analy->normals_constant = analy->state_p->position_constant &&
                                  !analy->state_p->sand_present;

    analy->auto_gray = TRUE;
    analy->int_point_labels = TRUE;
    analy->preferred_primal_source = PRIMAL;
    analy->have_hex_strains = FALSE;
    analy->render_particle_results_onto_bg_elements = TRUE;

    analy->showmat = TRUE;
    analy->result_source = ALL;
    analy->prev_result_source = ALL;
    analy->cur_state  = 0;
    analy->last_state = 0;
    analy->refresh = TRUE;
    analy->normals_smooth = TRUE;
    analy->render_mode = RENDER_MESH_VIEW;
    analy->mesh_view_mode = RENDER_HIDDEN;
    analy->interp_mode = NO_INTERP;
    analy->autoselect = FALSE;

    analy->manual_backface_cull = TRUE;
    analy->float_frac_size = DEFAULT_FLOAT_FRACTION_SIZE;
    analy->time_frac_size = DEFAULT_TIME_FRACTION_SIZE;
    analy->auto_frac_size = TRUE;
    analy->hidden_line_width = 1.0;
    analy->show_colorscale = TRUE;
    analy->use_global_mm = FALSE;
    analy->use_cumulative_mm = TRUE;
    analy->show_interp = FALSE;
    analy->use_snap = FALSE;
    analy->found_global_mm = FALSE;
    analy->strain_variety = INFINITESIMAL;
    analy->ref_surf = MIDDLE;
    analy->ref_frame = GLOBAL;
    analy->do_tensor_transform = FALSE;
    analy->show_plot_grid = TRUE;
    analy->show_plot_coords = TRUE;
    analy->color_plots = TRUE;
    analy->minmax_loc[0] = 'u';
    analy->minmax_loc[1] = 'l';
    analy->th_filter_width = 1;
    analy->th_filter_type = BOX_FILTER;
    if ( num_states > 0 )
    {
        analy->show_particle_class = FALSE;
    }
    create_class_list( &analy->bbox_source_qty, &analy->bbox_source,
                       MESH_P( analy ), 6, G_TRUSS, G_BEAM, G_TRI, G_QUAD,
                       G_HEX, G_PARTICLE );
    if ( analy->bbox_source == NULL )
    {
        popup_dialog( WARNING_POPUP,
                      "Failed to find object classes for initial bbox.- NO Element Classes Found" );
    }
    analy->vec_scale = 1.0;
    analy->vec_head_scale = 1.0;
    analy->mouse_mode = MOUSE_HILITE;
    analy->result_on_refs = TRUE;

    analy->contour_width = 1.0;
    analy->contour_cnt   = 0;
    analy->contour_vals  = NULL;

    analy->trace_width   = 1.0;
    analy->point_diam    = 0.0;
    analy->vectors_at_nodes = TRUE;
    analy->conversion_scale = 1.0;
    analy->zbias_beams = TRUE;
    analy->beam_zbias = DFLT_ZBIAS;
    analy->edge_zbias = DFLT_ZBIAS;
    analy->edge_width = 1.0;
    analy->z_buffer_lines = TRUE;
    analy->z_poly_offset = 0.0;
    analy->z_poly_last = 0.0;
    analy->hist_line_cnt = 0;

    analy->mesh_table->num_particle_classes = 0;
    analy->mesh_table->particle_class_names = NULL;

    analy->show_deleted_elements      = FALSE;
    analy->show_only_deleted_elements = FALSE;


    analy->echocmd     = TRUE;

    analy->analysis_type = TIME;
    analy->time_name     = NULL;

    analy->damage_result = FALSE;

    analy->p_histfile    = NULL;

    analy->vol_averaging = TRUE;
    analy->contours = NULL;

    set_contour_vals( 6, analy );

    VEC_SET( analy->displace_scale, 1.0, 1.0, 1.0 );

    /* Load in TI TOC if TI data is found */
    if(db_type == TAURUS)
    {
        env.ti_enable = FALSE;
    }
    if ( env.ti_enable )
    {
        num_entries = mc_ti_htable_search_wildcard(analy->db_ident, 0, FALSE,
                      "*", "NULL", "NULL",
                      NULL );

        wildcard_list=(char**) malloc( num_entries*sizeof(char *));

        if(wildcard_list == NULL)
        {
            return ALLOC_FAILED;
        }

        num_entries = mc_ti_htable_search_wildcard(analy->db_ident, 0, FALSE,
                      "M_", "NULL", "NULL",
                      wildcard_list );

        analy->ti_vars      = NULL;
        analy->ti_var_count = num_entries;
        analy->ti_vars      = (TI_Var *) malloc( num_entries*sizeof(TI_Var) );

        for ( i = 0; i < num_entries; i++ )
        {
            /* Extract the long name (with class info in name)
             * and short name of the TI variable.
             */
            status = mc_ti_get_metadata_from_name( wildcard_list[i],
                                                   class, &meshid, &state,
                                                   &matid, &superclass,
                                                   &isMeshvar, &isNodal);

            status = mc_ti_get_data_len( analy->db_ident, wildcard_list[i],
                                         &datatype, &datalength );

            analy->ti_vars[i].long_name  = (char *) strdup(wildcard_list[i]);
            analy->ti_vars[i].short_name = (char *) strdup( class );
            analy->ti_vars[i].superclass = superclass;
            analy->ti_vars[i].type       = datatype;
            analy->ti_vars[i].length     = datalength;
            analy->ti_vars[i].nodal      = isNodal;
        }

        analy->ti_data_found = TRUE;

        htable_delete_wildcard_list( num_entries, wildcard_list ) ;
        free(wildcard_list);
        wildcard_list = NULL;

        /* Get particle element names count */
        num_entries = mc_ti_htable_search_wildcard(analy->db_ident, 0, FALSE,
                      "particle_element", "NULL", "NULL",
                      NULL );
        if(num_entries > 0)
        {
            wildcard_list=(char**) malloc( num_entries*sizeof(char *));

            if(wildcard_list == NULL)
            {
                return ALLOC_FAILED;
            }

            /* Load particle element names from list if available */
            num_entries = mc_ti_htable_search_wildcard(analy->db_ident, 0, FALSE,
                          "particle_element", "NULL", "NULL",
                          wildcard_list );

            if ( num_entries>0 )
            {
                status = mc_ti_undef_class( analy->db_ident );
                analy->mesh_table->particle_class_names = NEW_N( char *, num_entries, "Particle Class Titles" );
                analy->mesh_table->num_particle_classes = num_entries;
                for ( i=0;
                        i<num_entries;
                        i++ )
                {
                    status = mc_ti_read_string( analy->db_ident, wildcard_list[i], particle_class_name );
                    analy->mesh_table->particle_class_names[i] = (char *) strdup( particle_class_name );
                }
                htable_delete_wildcard_list( num_entries, wildcard_list ) ;
                free(wildcard_list);
                wildcard_list = NULL;
            }
        }


        /* Get Modal analysis variables if they exist */
        num_entries = mc_ti_htable_search_wildcard(analy->db_ident, 0, FALSE,
                      "analysis_type", "NULL", "NULL",
                      NULL );

        if ( num_entries==1 )
        {
            char tmp_string[256];
            status = mc_ti_read_string( analy->db_ident, "analysis_type", tmp_string );
            if ( !strcmp( tmp_string, "modal" ) )
            {
                analy->analysis_type = MODAL;
            }

        }

        num_entries = mc_ti_htable_search_wildcard(analy->db_ident, 0, FALSE,
                      "changevar_timestep", "NULL", "NULL",
                      NULL );

        if ( num_entries==1 )
        {
            char tmp_string[256];
            status = mc_ti_read_string( analy->db_ident, "changevar_timestep", tmp_string );
            analy->time_name = strdup( tmp_string );
        }
        else
        {
            analy->ti_data_found = FALSE;
        }


    }

    /*
     * Loop over meshes to complete mesh-specific initializations.
     */

    mat_sz = 0;
    max_sz = 0;
    surf_max_sz = 0;

    max_face_qty = 0;

    for ( i = 0; i < analy->mesh_qty; i++ )
    {
        p_md = analy->mesh_table + i;

        cur_mesh_mat_qty = 0;
        cur_mesh_surf_qty = 0;

#ifdef NEWMILI
        stat = htable_get_data( p_md->class_table,
                                (void ***) &class_array,
                                &class_qty);
#else
        class_qty = htable_get_data( p_md->class_table,
                                     (void ***) &class_array);
#endif

        /*
         * Make an initial pass to process G_MAT classes.  This generates
         * data necessary for element classes.
         */
        surface_sz         = 0;
        p_md->brick_qty    = 0;
        p_md->shell_qty    = 0;
        p_md->beam_qty     = 0;
        p_md->truss_qty    = 0;

        p_md->particle_qty = 1;  /* Force particle allocation for now since we
                                      * do not have a superclass for particles
                                      */

        p_md->num_elem_classes = 0;

        for ( j = 0; j < class_qty; j++ )
        {
            p_mocd = class_array[j];
            if ( IS_ELEMENT_SCLASS( p_mocd->superclass ) )
                elem_class_qty++;

            if ( p_mocd->superclass == G_MAT )
            {
                if ( p_mocd->qty > mat_sz )
                    mat_sz = p_mocd->qty;
                if ( mat_sz > max_sz )
                    max_sz = mat_sz;

                /*
                 * Should only be one mat class, but be defensive...
                 * If there's more than one class, we'll have problems
                 * because references to materials for elements assume
                 * a single material class, (i.e., the material numbers
                 * only identify a material, not a class).
                 */
                cur_mesh_mat_qty += p_mocd->qty;
            }

            if ( p_mocd->superclass == G_SURFACE )
            {
                if ( p_mocd->qty > surface_sz )
                    surface_sz = p_mocd->qty;
                if ( surface_sz > surf_max_sz )
                    surf_max_sz = surface_sz;

                cur_mesh_surf_qty += p_mocd->qty;
            }

            if ( p_mocd->superclass == G_QUAD )
            {
                p_md->shell_qty += p_mocd->qty;
                shell_qty       +=  p_md->shell_qty;
            }
            if ( p_mocd->superclass == G_HEX && !is_particle_class( analy, p_mocd->superclass, p_mocd->short_name ) )
            {
                p_md->brick_qty +=  p_mocd->qty;
                p_brick_class    =  p_mocd;
                brick_qty       +=  p_md->brick_qty;
            }
            if ( (p_mocd->superclass == G_HEX && is_particle_class( analy, p_mocd->superclass, p_mocd->short_name )) ||
                    p_mocd->superclass == G_PARTICLE )
            {
                p_ml_class         =   p_mocd;
                p_md->particle_qty +=  p_mocd->qty;
                particle_qty       +=  p_mocd->qty;
            }
            if ( p_mocd->superclass == G_BEAM )
            {
                p_md->beam_qty += p_mocd->qty;
                beam_qty       += p_md->beam_qty;
            }
            if ( p_mocd->superclass == G_TRUSS )
            {
                p_md->truss_qty += p_mocd->qty;
                truss_qty       += p_md->truss_qty;
            }
            if ( p_mocd->superclass == G_TET )
            {
                p_md->tet_qty += p_mocd->qty;
                tet_qty += p_md->tet_qty;
            }
        }

        /* Allocate material quantity-driven arrays for mesh. */
        p_md->hide_material = NEW_N( unsigned char, cur_mesh_mat_qty,
                                     "Material visibility" );
        p_md->disable_material = NEW_N( unsigned char, cur_mesh_mat_qty,
                                        "Material-based result disabling" );

        for (i=0;
                i<cur_mesh_mat_qty;
                i++)
        {
            p_md->hide_material[i]    = '\0';
            p_md->disable_material[i] = '\0';
        }

        p_md->hide_brick    = NULL;
        p_md->disable_brick = NULL;
        if ( p_md->brick_qty >0 )
        {
            p_md->hide_brick = NEW_N( unsigned char, cur_mesh_mat_qty,
                                      "Brick visibility" );
            p_md->disable_brick = NEW_N( unsigned char, cur_mesh_mat_qty,
                                         "Brick-based result disabling" );
            p_md->exclude_brick = NEW_N( unsigned char, cur_mesh_mat_qty,
                                         "Brick-based result exclusion" );

            /* Allocate space for the per element qualtities */
            p_md->hide_brick_elem = NEW_N( unsigned char, brick_qty,
                                           "Brick element visibility" );
            p_md->disable_brick_elem = NEW_N( unsigned char, brick_qty,
                                              "Brick-based element result disabling" );
            p_md->exclude_brick_elem = NEW_N( unsigned char, brick_qty,
                                              "Brick-based element result exclution" );

            /* Initialize brick arrays */

            for (i=0;
                    i<brick_qty;
                    i++)
            {
                p_md->hide_brick_elem[i]    = '\0';
                p_md->disable_brick_elem[i] = '\0';
                p_md->exclude_brick_elem[i] = '\0';
            }
        }

        p_md->hide_shell    = NULL;
        p_md->disable_shell = NULL;
        if ( p_md->shell_qty >0 )
        {
            p_md->hide_shell = NEW_N( unsigned char, cur_mesh_mat_qty,
                                      "Shell visibility" );
            p_md->disable_shell = NEW_N( unsigned char, cur_mesh_mat_qty,
                                         "Shell-based result disabling" );
            p_md->exclude_shell = NEW_N( unsigned char, cur_mesh_mat_qty,
                                         "Shell-based result exclusion" );

            /* Allocate space for the per element qualtities */
            p_md->hide_shell_elem = NEW_N( unsigned char, shell_qty,
                                           "Shell element visibility" );
            p_md->disable_shell_elem = NEW_N( unsigned char, shell_qty,
                                              "Shell-based element result disabling" );
            p_md->exclude_shell_elem = NEW_N( unsigned char, shell_qty,
                                              "Shell-based element result exclution" );
            /* Initialize shell arrays */

            for (i=0;
                    i<shell_qty;
                    i++)
            {
                p_md->hide_shell_elem[i]     = '\0';
                p_md->disable_shell_elem[i]  = '\0';
                p_md->exclude_shell_elem[i]  = '\0';
            }
        }

        p_md->hide_beam    = NULL;
        p_md->disable_beam = NULL;
        if ( p_md->beam_qty >0 )
        {
            p_md->hide_beam = NEW_N( unsigned char, cur_mesh_mat_qty,
                                     "Beam visibility" );
            p_md->disable_beam = NEW_N( unsigned char, cur_mesh_mat_qty,
                                        "Beam-based result disabling" );
            p_md->exclude_beam = NEW_N( unsigned char, cur_mesh_mat_qty,
                                        "Beam-based result exclusion" );

            /* Allocate space for the per element qualtities */
            p_md->hide_beam_elem = NEW_N( unsigned char, beam_qty,
                                          "Beam element visibility" );
            p_md->disable_beam_elem = NEW_N( unsigned char, beam_qty,
                                             "Beam-based element result disabling" );
            p_md->exclude_beam_elem = NEW_N( unsigned char, beam_qty,
                                             "Beam-based element result exclution" );

            /* Initialize beam arrays */

            for (i=0;
                    i<beam_qty;
                    i++)
            {
                p_md->hide_beam_elem[i]     = '\0';
                p_md->disable_beam_elem[i]  = '\0';
                p_md->exclude_beam_elem[i]  = '\0';
            }
        }


        p_md->hide_truss    = NULL;
        p_md->disable_truss = NULL;
        if ( p_md->truss_qty >0 )
        {
            p_md->hide_truss = NEW_N( unsigned char, cur_mesh_mat_qty,
                                      "Truss visibility" );
            p_md->disable_truss = NEW_N( unsigned char, cur_mesh_mat_qty,
                                         "Truss-based result disabling" );
            p_md->exclude_truss = NEW_N( unsigned char, cur_mesh_mat_qty,
                                         "Truss-based result exclusion" );

            /* Allocate space for the per element qualtities */
            p_md->hide_truss_elem = NEW_N( unsigned char, truss_qty,
                                           "Truss element visibility" );
            p_md->disable_truss_elem = NEW_N( unsigned char, truss_qty,
                                              "Truss-based element result disabling" );
            p_md->exclude_truss_elem = NEW_N( unsigned char, truss_qty,
                                              "Truss-based element result exclution" );

            /* Initialize Truss arrays */

            for (i=0;
                    i<truss_qty;
                    i++)
            {
                p_md->hide_truss_elem[i]     = '\0';
                p_md->disable_truss_elem[i]  = '\0';
                p_md->exclude_truss_elem[i]  = '\0';
            }
        }

        p_md->hide_particle    = NULL;
        p_md->disable_particle = NULL;
        p_md->particle_mats    = NULL;
        if ( p_md->particle_qty >0 )
        {
            p_md->hide_particle    = NEW_N( unsigned char, cur_mesh_mat_qty,
                                            "Particle visibility" );
            p_md->particle_mats    = NEW_N( Bool_type, cur_mesh_mat_qty,
                                            "Particle material list" );
            p_md->disable_particle = NEW_N( unsigned char, cur_mesh_mat_qty,
                                            "Particle-based result disabling" );

            /* Allocate space for the per element qualtities */
            p_md->hide_particle_elem = NEW_N( unsigned char, particle_qty,
                                              "Particle element visibility" );
            p_md->disable_particle_elem = NEW_N( unsigned char, particle_qty,
                                                 "Particle-based element result disabling" );
            p_md->exclude_particle_elem = NEW_N( unsigned char, particle_qty,
                                                 "Particle-based element result exclution" );
            /* Initialize Particle arrays */

            for (i=0;
                    i<particle_qty;
                    i++)
            {
                p_md->hide_particle_elem[i]     = '\0';
                p_md->disable_particle_elem[i]  = '\0';
                p_md->exclude_particle_elem[i]  = '\0';
            }
        }

        /* Allocate data structures for By-Class Selections */

        p_md->by_class_select = NEW_N( Class_Select, elem_class_qty,
                                       "Class selection data" );

        for ( i=0;
                i<class_qty;
                i++ )
        {
            sclass = class_array[i]->superclass;
            if ( IS_ELEMENT_SCLASS( sclass ) )
            {
                p_md->by_class_select[class_index++].p_class = class_array[i];
                p_md->qty_class_selections++;
            }
            if ( class_index>=elem_class_qty )
                break;
        }
        for ( i=0;
                i< p_md->qty_class_selections;
                i++ )
        {
            int num_elems = p_md->by_class_select[i].p_class->qty;
            p_md->by_class_select[i].hide_class = NEW_N( unsigned char, cur_mesh_mat_qty,
                                                  "Class visibility" );
            p_md->by_class_select[i].disable_class = NEW_N( unsigned char, cur_mesh_mat_qty,
                    "Class-based result disabling" );
            p_md->by_class_select[i].exclude_class = NEW_N( unsigned char, cur_mesh_mat_qty,
                    "Class-based result exclusion" );

            for (j=0;
                    j<cur_mesh_mat_qty;
                    j++)
            {
                p_md->by_class_select[i].hide_class[j]     = '\0';
                p_md->by_class_select[i].disable_class[j]  = '\0';
                p_md->by_class_select[i].exclude_class[j]  = '\0';
            }

            /* Allocate space for the per element qualtities */
            p_md->by_class_select[i].hide_class_elem = NEW_N( unsigned char, num_elems,
                    "Class element visibility" );
            p_md->by_class_select[i].disable_class_elem = NEW_N( unsigned char, num_elems,
                    "Class-based element result disabling" );
            p_md->by_class_select[i].exclude_class_elem = NEW_N( unsigned char, num_elems,
                    "Class-based element result exclution" );

            /* Initialize class arrays */

            for (j=0;
                    j<num_elems;
                    j++)
            {
                p_md->by_class_select[i].hide_class_elem[j]     = '\0';
                p_md->by_class_select[i].disable_class_elem[j]  = '\0';
                p_md->by_class_select[i].exclude_class_elem[j]  = '\0';
            }
        }

        num_nodes = p_md->node_geom->qty;

        /* Allocate space for NODE disable array */

        p_md->disable_nodes = NEW_N( short, p_md->node_geom->qty,
                                     "Node result exclusion" );

        /* Initialize Node array */
        for (i=0;
                i<p_md->node_geom->qty;
                i++)
        {
            p_md->disable_nodes[i] = FALSE;
        }

        p_md->disable_nodes_init = FALSE;

        /* Initialize per-material arrays */

        for (i=0;
                i<cur_mesh_mat_qty;
                i++)
        {
            if ( p_md->brick_qty>0)
            {
                p_md->hide_brick[i]     = '\0';
                p_md->disable_brick[i]  = '\0';
                p_md->exclude_brick[i]  = '\0';
            }

            if ( p_md->shell_qty>0)
            {
                p_md->hide_shell[i]     = '\0';
                p_md->disable_shell[i]  = '\0';
                p_md->exclude_shell[i]  = '\0';
            }

            if ( p_md->beam_qty>0)
            {
                p_md->hide_beam[i]     = '\0';
                p_md->disable_beam[i]  = '\0';
                p_md->exclude_beam[i]  = '\0';
            }

            if ( p_md->truss_qty>0)
            {
                p_md->hide_truss[i]     = '\0';
                p_md->disable_truss[i]  = '\0';
                p_md->exclude_truss[i]  = '\0';
            }

            if ( p_md->particle_qty>0)
            {
                p_md->hide_particle[i]     = '\0';
                p_md->disable_particle[i]  = '\0';
                p_md->particle_mats[i]     = FALSE;
            }
        }

        for ( i = 0; i < 3; i++ )
            p_md->mtl_trans[i] = NEW_N( float, cur_mesh_mat_qty,
                                        "Material translations" );
        p_md->material_qty = cur_mesh_mat_qty;


        analy->free_nodes_mats = NEW_N( int, cur_mesh_mat_qty,
                                        "Free Node material list" );
        memset( analy->free_nodes_mats, 0, cur_mesh_mat_qty );
        analy->free_nodes_scale_factor      = 2.0;
        analy->free_nodes_sphere_res_factor = 4;
        analy->free_nodes_mass_scaling      = 0;   /* Default=Disabled */
        analy->free_nodes_vol_scaling       = 0;   /* Default=Disabled */
        analy->free_nodes_cut_width         = 0;   /* Default=0 */
        analy->show_particle_cut            = FALSE;
        analy->particle_nodes_hide_background = FALSE;
        analy->particle_nodes_found           = FALSE;

        analy->hide_edges_by_mat              = FALSE;
        analy->free_nodes_list                = NULL;
        analy->free_nodes_vals                = NULL;

        analy->extreme_node_mm                = NULL;

#ifdef TIME_OPEN_ANALYSIS
        printf( "Timing face table creation...\n" );
        manage_timer( 0, 0 );
#endif
        face_qty = 0;

        /* Now handle the other classes. */
        for ( j = 0; j < class_qty; j++ )
        {
            p_mocd = class_array[j];

            /* Class-specific initializations.
             *
             * Face tables are created for each volumetric element
             * class.  Since this won't cull faces shared between elements
             * of the same superclass but different classes, it may be
             * preferable to create a table only for each volumetric
             * element superclass, merging classes derived from the same
             * superclass.  That would require additional structures to
             * map elements unique within a superclass to their class-
             * specific identities, and may not be worth the overhead
             * if meshes typically have only one class per superclass.
             */
            switch ( p_mocd->superclass )
            {
            case G_UNIT:
            case G_NODE:
            case G_TRUSS:
            case G_BEAM:
                if ( p_mocd->qty > max_sz )
                    max_sz = p_mocd->qty;
                break;

            case G_TRI:
                if ( p_mocd->qty > max_sz )
                    max_sz = p_mocd->qty;

                create_tri_blocks( p_mocd, analy );

                p_vd = NEW( Visibility_data,
                            "Elem class Visibility_data" );
                p_mocd->p_vis_data = p_vd;

                face_qty += p_mocd->qty;

                /* Allocate space for normals. */
                if ( analy->dimension == 3 )
                    for ( k = 0; k < 3; k++ )
                        for ( l = 0; l < 3; l++ )
                            p_vd->face_norm[k][l] =
                                NEW_N( float, p_mocd->qty,
                                       "Tri normals" );

                break;

            case G_QUAD:
                if ( p_mocd->qty > max_sz )
                    max_sz = p_mocd->qty;

                create_quad_blocks( p_mocd, analy );

                p_vd = NEW( Visibility_data,
                            "Elem class Visibility_data" );
                p_mocd->p_vis_data = p_vd;

                face_qty += p_mocd->qty;

                /* Allocate space for normals. */
                if ( analy->dimension == 3 )
                    for ( k = 0; k < 4; k++ )
                        for ( l = 0; l < 3; l++ )
                            p_vd->face_norm[k][l] =
                                NEW_N( float, p_mocd->qty,
                                       "Quad normals" );

                break;

            case G_TET:
                if ( p_mocd->qty > max_sz )
                    max_sz = p_mocd->qty;

                create_tet_blocks( p_mocd, analy );

                p_vd = NEW( Visibility_data,
                            "Elem class Visibility_data" );
                p_mocd->p_vis_data = p_vd;

                p_vd->adjacency = NEW_N( int *, 4,
                                         "Tet adjacency array" );
                for ( k = 0; k < 4; k++ )
                    p_vd->adjacency[k] = NEW_N( int, p_mocd->qty,
                                                "Tet adjacency" );
                p_vd->visib = NEW_N( unsigned char, p_mocd->qty,
                                     "Tet visibility" );

                create_tet_adj( p_mocd, analy );
                set_tet_visibility( p_mocd, analy );
                get_tet_faces( p_mocd, analy );

                face_qty += p_mocd->p_vis_data->face_cnt;

                break;

            case G_HEX:
                if ( p_mocd->qty > max_sz )
                    max_sz = p_mocd->qty;

                create_hex_blocks( p_mocd, analy );

                p_vd = NEW( Visibility_data,
                            "Elem class Visibility_data" );
                p_mocd->p_vis_data = p_vd;

                p_vd->adjacency = NEW_N( int *, 6,
                                         "Hex adjacency array" );
                for ( k = 0; k < 6; k++ )
                    p_vd->adjacency[k] = NEW_N( int, p_mocd->qty,
                                                "Hex adjacency" );
                p_vd->visib = NEW_N( unsigned char, p_mocd->qty,
                                     "Hex visibility" );

                p_vd->visib_damage = NEW_N( unsigned char, p_mocd->qty,
                                            "Hex Damage visibility" );

                if ( is_particle_class( analy, p_mocd->superclass, p_mocd->short_name ) )
                {
                    init_particle_visibility( p_mocd, analy );

                }
                else
                {
                    init_hex_visibility( p_mocd, analy );

                    create_hex_adj( p_mocd, analy );
                    set_hex_visibility( p_mocd, analy );
                    get_hex_faces( p_mocd, analy );

                    face_qty += p_mocd->p_vis_data->face_cnt;
                }
                break;
            case G_PYRAMID:
                if ( p_mocd->qty > max_sz)
                {
                    max_sz = p_mocd->qty;
                }

                create_pyramid_blocks( p_mocd, analy );

                p_vd = NEW( Visibility_data,
                            "Elem class Visibility_data" );
                p_mocd->p_vis_data = p_vd;

                p_vd->adjacency = NEW_N( int *, 5,
                                         "Hex adjacency array" );
                for ( k = 0; k < 5; k++ )
                    p_vd->adjacency[k] = NEW_N( int, p_mocd->qty,
                                                "Hex adjacency" );
                p_vd->visib = NEW_N( unsigned char, p_mocd->qty,
                                     "Hex visibility" );

                create_pyramid_adj( p_mocd, analy );
                set_pyramid_visibility( p_mocd, analy );
                get_pyramid_faces( p_mocd, analy );
                face_qty += p_mocd->p_vis_data->face_cnt;

                break;

            case G_SURFACE:

                for ( k = 0; k < p_mocd->qty; k++ )
                    max_sz += p_mocd->qty;

                face_qty += p_mocd->qty;

                for( surf = 0; surf < p_mocd->qty; surf++ )
                {
                    p_surface = p_mocd->objects.surfaces + surf;
                    facets = p_surface->facet_qty;

                    p_vd = NEW( Visibility_data,
                                "Surface Visibility_data" );
                    p_surface->p_vis_data = p_vd;

                    /* Allocate space for normals. */
                    for ( k = 0; k < 4; k++ )
                        for ( l = 0; l < 3; l++ )
                            p_vd->face_norm[k][l] =
                                NEW_N( float, facets, "Surface normals" );
                }

                /* Allocate surface quantity-driven arrays for mesh. */
                p_md->hide_surface = NEW_N( unsigned char,
                                            cur_mesh_surf_qty,
                                            "Surface visibility" );
                p_md->disable_surface = NEW_N( unsigned char,
                                               cur_mesh_surf_qty,
                                               "Surface-based result disabling" );
                p_md->surface_qty = cur_mesh_surf_qty;

                break;

            case G_PARTICLE:
                if ( p_mocd->qty > max_sz )
                    max_sz = p_mocd->qty;

                p_vd = NEW( Visibility_data,
                            "Elem class Visibility_data" );
                p_mocd->p_vis_data = p_vd;

                p_vd->adjacency = NULL;

                p_vd->visib = NEW_N( unsigned char, p_mocd->qty,
                                     "Particle visibility" );

                p_vd->visib_damage = NEW_N( unsigned char, p_mocd->qty,
                                            "Particle Damage visibility" );

                init_particle_visibility( p_mocd, analy );
                break;
            default:
                ;
            }
        }

#ifdef TIME_OPEN_ANALYSIS
        manage_timer( 0, 1 );
        putc( (int) '\n', stdout );
#endif

        free( class_array );
    }

    if ( face_qty > max_face_qty )
        max_face_qty = face_qty;

    /* Generate list of banned names */
    generate_banned_names_list( analy );

    /* Save max quantity of materials. */
    analy->max_mesh_mat_qty = mat_sz;
    analy->mat_labels_active = TRUE;
    analy->maxLabelLength = 0;

    /* Generate material name/num hash tables */
    Hash_table *forNames;
    Hash_table *revNames;
    char** sortedNames = malloc(analy->max_mesh_mat_qty * sizeof(char*));
    Hash_table *forNums;
    Hash_table *revNums;
    char** sortedNums = malloc(analy->max_mesh_mat_qty * sizeof(char*));
    forNames = htable_create( 1001 );
    revNames = htable_create( 1001 );
    forNums = htable_create( 1001 );
    revNums = htable_create( 1001 );

    Htable_entry *tempEnt;
    int pos2;
    analy->conflict_messages = malloc(analy->max_mesh_mat_qty * (sizeof(char*)));
    analy->num_messages = 0;
    char teststr[20];
    char merged[label_length];
    char temp[label_length];
    char message[120];

    if(analy->mat_labels_active && env.ti_enable)
    {
		for(pos2 = 0; pos2 < analy->max_mesh_mat_qty; pos2++)
        {
			//check if name exists
			teststr[0] = '\0';
			int num_items_read = 0;
			int status = 0;
			sprintf(teststr,"MAT_NAME_%d",pos2+1);
			char *test;
			char *test2;
			test = malloc(label_length * sizeof(char));
			test2 = malloc(label_length * sizeof(char));
			status = mc_ti_read_string(analy->db_ident, teststr, (void*) test);
			char *str;
			char *str2;
			str = malloc(10 * sizeof(char));
			str2 = malloc(10 * sizeof(char));
			sprintf(str,"%d",pos2+1);//if so then print name
			sprintf(str2,"%d",pos2+1);//if so then print name
			sprintf(test2,"%s",str2);
			if (status == OK)
            {
				int bpos = 0;
				for(bpos = 0; bpos < analy->num_banned_names; bpos ++)
                {
				    if(strcmp(analy->banned_names[bpos],test) == 0)
                    {
						merged[0]='\0';
						temp[0]='\0';
						message[0]='\0';
						sprintf(temp,"%s",test);
						sprintf(merged,"%s%s","_",test);
						sprintf(test,"%s",merged);
						sprintf(message, "Conflicting Material Label: \"%s\" is now \"%s\"\n", temp, merged);
						analy->conflict_messages[analy->num_messages] = malloc(120 * sizeof(char));
						sprintf(analy->conflict_messages[analy->num_messages], "%s", message);
						analy->num_messages++;
					}
				}
			}
			else
            {
				sprintf(test,"%s",str);
			}

         	htable_add_entry_data(revNames,str ,ENTER_UNIQUE,(void *) test);
			htable_add_entry_data(forNames,test ,ENTER_UNIQUE,(void *) str);
			sortedNames[pos2] = malloc(label_length * sizeof(char));
			sprintf(sortedNames[pos2],"%s",test);
			int curlen = strlen(test);
			if(curlen > analy->maxLabelLength){
				analy->maxLabelLength = curlen;
			}
			//ADD TO NAME ALPHA LIST
			htable_add_entry_data(revNums,str2 ,ENTER_UNIQUE,(void *) test2);
			htable_add_entry_data(forNums,test2 ,ENTER_UNIQUE,(void *) str2);
			sortedNums[pos2] = malloc(label_length * sizeof(char));
			sprintf(sortedNums[pos2],"%s",test2);
		}
    }
    else
    {
		for(pos2 = 0; pos2 < analy->max_mesh_mat_qty; pos2++)
        {
			//check if name exists
			int num_items_read = 0;
			int status = 0;
			char *test;
			char *test2;
			test = malloc(label_length * sizeof(char));
			test2 = malloc(label_length * sizeof(char));
			char *str;
			char *str2;
			str = malloc(10 * sizeof(char));
			str2 = malloc(10 * sizeof(char));
			sprintf(str2,"%d",pos2+1);//if so then print name
			sprintf(test2,"%s",str2);
			sprintf(str,"%d",pos2+1);//if so then print name
			sprintf(test,"%s",str);

         	htable_add_entry_data(revNames,str ,ENTER_UNIQUE,(void *) test);
			htable_add_entry_data(forNames,test ,ENTER_UNIQUE,(void *) str);
			sortedNames[pos2] = malloc(label_length * sizeof(char));
			sprintf(sortedNames[pos2],"%s",test);
			//ADD TO NAME ALPHA LIST
			htable_add_entry_data(revNums,str2 ,ENTER_UNIQUE,(void *) test2);
			htable_add_entry_data(forNums,test2 ,ENTER_UNIQUE,(void *) str2);
			sortedNums[pos2] = malloc(label_length * sizeof(char));
			sprintf(sortedNums[pos2],"%s",test2);
			int curlen = strlen(test);
			if(curlen > analy->maxLabelLength)
            {
				analy->maxLabelLength = curlen;
			}
		}
    }

    /* Sort names and nums and store in analysis struct */
    qsort(sortedNames, analy->max_mesh_mat_qty, sizeof(char*), (void*)alphanum_cmp);
    analy->sorted_names = sortedNames;
    analy->mat_names = forNames;
    analy->mat_names_reversed = revNames;

    qsort(sortedNums, analy->max_mesh_mat_qty, sizeof(char*), (void*)alphanum_cmp);
    analy->sorted_nums = sortedNums;
    analy->mat_nums = forNums;
    analy->mat_nums_reversed = revNums;

    // Make sure we get rid of dangling pointers
    // DO NOT free this as it is now your analy->sorted_names pointer
    // Simply setting it to NULL makes sure w do not have a dangling
    // pointer.
    sortedNames = NULL;
    sortedNums = NULL;
    

    //default setting area
    analy->sorted_labels = analy->sorted_names;
    analy->mat_labels = analy->mat_names;
	analy->mat_labels_reversed = analy->mat_names_reversed;
		
    //init color storage
    analy->preview_mode = False;
    //last colors
	analy->lastColorActive = False;
	analy->last_ambient = malloc(analy->max_mesh_mat_qty * sizeof(float*));
	analy->last_diffuse = malloc(analy->max_mesh_mat_qty * sizeof(float*));
	analy->last_specular = malloc(analy->max_mesh_mat_qty * sizeof(float*));
	analy->last_emission = malloc(analy->max_mesh_mat_qty * sizeof(float*));
	analy->last_shininess = malloc(analy->max_mesh_mat_qty * sizeof(float));
	//default colors
	analy->defaultColorActive = False;
	analy->default_ambient = malloc(analy->max_mesh_mat_qty * sizeof(float*));
	analy->default_diffuse = malloc(analy->max_mesh_mat_qty * sizeof(float*));
	analy->default_specular = malloc(analy->max_mesh_mat_qty * sizeof(float*));
	analy->default_emission = malloc(analy->max_mesh_mat_qty * sizeof(float*));
	analy->default_shininess = malloc(analy->max_mesh_mat_qty * sizeof(float));

	//init loop
	int colorPos = 0;
	for(colorPos = 0; colorPos < analy->max_mesh_mat_qty; colorPos++){
		//last colors
		analy->last_ambient[colorPos] = malloc(3 * sizeof(float));
		analy->last_ambient[colorPos][0] = -1.0;
		analy->last_ambient[colorPos][1] = -1.0;
		analy->last_ambient[colorPos][2] = -1.0;
		analy->last_diffuse[colorPos] = malloc(4 * sizeof(float));
		analy->last_diffuse[colorPos][0] = -1.0;
		analy->last_diffuse[colorPos][1] = -1.0;
		analy->last_diffuse[colorPos][2] = -1.0;
		analy->last_diffuse[colorPos][3] = -1.0;
		analy->last_specular[colorPos] = malloc(3 * sizeof(float));
		analy->last_specular[colorPos][0] = -1.0;
		analy->last_specular[colorPos][1] = -1.0;
		analy->last_specular[colorPos][2] = -1.0;
		analy->last_emission[colorPos] = malloc(3 * sizeof(float));
		analy->last_emission[colorPos][0] = -1.0;
		analy->last_emission[colorPos][1] = -1.0;
		analy->last_emission[colorPos][2] = -1.0;
		analy->last_shininess[colorPos] = -1.0;
		//default colors
		analy->default_ambient[colorPos] = malloc(3 * sizeof(float));
		analy->default_ambient[colorPos][0] = -1.0;
		analy->default_ambient[colorPos][1] = -1.0;
		analy->default_ambient[colorPos][2] = -1.0;
		analy->default_diffuse[colorPos] = malloc(4 * sizeof(float));
		analy->default_diffuse[colorPos][0] = -1.0;
		analy->default_diffuse[colorPos][1] = -1.0;
		analy->default_diffuse[colorPos][2] = -1.0;
		analy->default_diffuse[colorPos][3] = -1.0;
		analy->default_specular[colorPos] = malloc(3 * sizeof(float));
		analy->default_specular[colorPos][0] = -1.0;
		analy->default_specular[colorPos][1] = -1.0;
		analy->default_specular[colorPos][2] = -1.0;
		analy->default_emission[colorPos] = malloc(3 * sizeof(float));
		analy->default_emission[colorPos][0] = -1.0;
		analy->default_emission[colorPos][1] = -1.0;
		analy->default_emission[colorPos][2] = -1.0;
		analy->default_shininess[colorPos] = -1.0;
	}

	//END NEW

    /* Save max quantity of surfaces. */
    analy->max_mesh_surf_qty = surface_sz;

    /*
     * Allocate space for temporary results. Allocate as a contiguous
     * array so the space can do double duty as storage for aggregate
     * primals of up to six components.
     */
#ifdef NEW_TMP_RESULT_USE
    init_data_buffers( max_sz );
#endif
#ifndef NEW_TMP_RESULT_USE
    analy->tmp_result[0] = NEW_N( float, TEMP_RESULT_ARRAY_QTY * max_sz * 2,
                                  "Tmp result cache" );
    for ( i = 1; i < TEMP_RESULT_ARRAY_QTY ; i++ )
        analy->tmp_result[i] = analy->tmp_result[i - 1] + max_sz * 2;
#endif
    analy->max_result_size = max_sz;

    /* Get the normals. */
    if ( analy->dimension == 3 )
    {
        /* Allocate the node normals working array. */
        for ( i = 0; i < 3; i++ )
            analy->node_norm[i] = NEW_N( float, max_sz, "Node normals" );

#ifdef TIME_OPEN_ANALYSIS
        printf( "Timing normals computation...\n" );
        manage_timer( 0, 0 );
#endif

        compute_normals( analy );

#ifdef TIME_OPEN_ANALYSIS
        manage_timer( 0, 1 );
        putc( (int) '\n', stdout );
#endif
    }

    /* Compute an edge list for fast object display. */

#ifdef TIME_OPEN_ANALYSIS
    printf( "Timing edge set determination...\n" );
    manage_timer( 0, 0 );
#endif

    get_mesh_edges( 0, analy );

#ifdef TIME_OPEN_ANALYSIS
    manage_timer( 0, 1 );
    putc( (int) '\n', stdout );
#endif

    /* Set initial display drawing function. */
    analy->update_display = draw_mesh_view;
    /*analy->update_display = quick_draw_mesh_view; */
    analy->display_mode_refresh = update_vis;
    analy->check_mod_required = mod_required_mesh_mode;

    /* Set render mode based on the number of potentially visible faces. */
    if ( max_face_qty > 10000 )
        analy->mesh_view_mode = RENDER_FILLED;

    /* Enable one-time warning if zero-volume-elements encountered. */
    reset_zero_vol_warning();

    /* Set initial reference state for nodes to mesh definition data. */
    analy->cur_ref_state_dataDp  = NULL;
    analy->center_view           = NO_CENTER;
    analy->suppress_updates      = FALSE;
    analy->show_sph_ghost        = TRUE;
    analy->selectonmat           = FALSE;

    for ( i = 0; i < MAX_PATHS; i++ )
    {
        analy->paths[i]        = NULL;
        analy->paths_set[i]    = FALSE;
    }

    set_ref_state( analy, 0 );

    /* Set a pointer to ML class if it exists */
    if ( p_ml_class )
    {
        p_mesh = MESH_P( analy );
        p_mesh->p_ml_class    = p_ml_class;
    }

    /* Get the metadata from the A file */
    mc_get_metadata( analy->db_ident,  analy->mili_version,   analy->mili_host,
                     analy->mili_arch, analy->mili_timestamp, analy->xmilics_version );

    if ( env.model_history_logging )
    {

        if ( !analy->p_histfile )
        {
            time(&curtime);
            timeinfo = localtime(&curtime);

            sprintf(timestr, "_%d.%d.%d_%d:%d:%d", timeinfo->tm_mon
                                                , timeinfo->tm_mday
                                                , timeinfo->tm_year -100
                                                , timeinfo->tm_hour
                                                , timeinfo->tm_min
                                                , timeinfo->tm_sec);

            sprintf(timestamp, "%s", asctime(timeinfo));

            strcpy (hist_fname, ".");
            strcat( hist_fname, analy->root_name );
            strcat( hist_fname, timestr);
            strcat( hist_fname, ".grizhist" );
            analy->p_histfile = fopen( hist_fname, "at");
            strcpy(analy->hist_fname, hist_fname);
            fseek(analy->p_histfile, 0L, SEEK_END);
            pos = ftell(analy->p_histfile);
            fprintf(analy->p_histfile, "#");
            fprintf(analy->p_histfile, "%s", timestamp);
            if(pos == 0)
            {
                fprintf(analy->p_histfile, "%s", header);
                fflush(analy->p_histfile);
            }

        }
    }

    return TRUE;
}



/************************************************************
 * TAG( close_analysis )
 *
 * Free up all the memory absorbed by an analysis.
 */
void
close_analysis( Analysis *analy )
{
    int i;
    Classed_list *p_cl;
    Triangle_poly *p_tp;
    char delth_tokens[2][TOKENLENGTH] =
    {
        "delth", "all"
    };

    fr_state2( analy->state_p, analy );
    analy->state_p = NULL;

    free( analy->bbox_source );

    DELETE_LIST( analy->refl_planes );
    free_matl_exp();
    if ( analy->classarray != NULL )
        free( analy->classarray );
    free( analy->tmp_result[0] );
    if ( analy->dimension == 3 )
    {
        /* Free the node normals working array. */
        for ( i = 2; i >= 0; i-- )
            free( analy->node_norm[i] );
    }
    DELETE_LIST( analy->result_mm_list );
    if ( analy->contour_cnt > 0 )
        free( analy->contour_vals );
    free_contours( analy );
    free_trace_points( analy );
    if ( analy->trace_disable != NULL )
        free( analy->trace_disable );
    analy->trace_disable_qty = 0;
    DELETE_LIST( analy->isosurf_poly );
    DELETE_LIST( analy->cut_planes );
    for ( p_cl = analy->cut_poly_lists; p_cl != NULL; NEXT( p_cl ) )
    {
        p_tp = (Triangle_poly *) p_cl->list;
        DELETE_LIST( p_tp );
    }
    DELETE_LIST( analy->cut_poly_lists );
    delete_vec_points( analy );
    if ( analy->vector_srec_map != NULL )
    {
        for ( i = 0; i < analy->qty_srec_fmts; i++ )
            if ( analy->vector_srec_map[i].qty != 0 )
                free( analy->vector_srec_map[i].list );

        free( analy->vector_srec_map );
    }

    DELETE_LIST( analy->selected_objects );

    /* Clean up time history resources. */
    if ( analy->srec_formats != NULL )
    {
        free( analy->srec_formats );
        analy->srec_formats = NULL;
    }
    if ( analy->format_blocks != NULL )
    {
        for ( i = 0; i < analy->qty_srec_fmts; i++ )
            free( analy->format_blocks[i].list );
        free( analy->format_blocks );
        analy->format_blocks = NULL;
    }

    /*
     * Call this directly rather than via parse_command() since
     * parse_command() will attempt to redraw after the plots have
     * been deleted.
     */
    delete_time_series( sizeof( delth_tokens ) / sizeof( delth_tokens[0] ),
                        delth_tokens, analy );
    if ( analy->current_plots != NULL )
        popup_dialog( WARNING_POPUP,
                      "Current plot list non-NULL after clear." );
    remove_unused_results( &analy->series_results );
    if ( analy->series_results != NULL )
        popup_dialog( WARNING_POPUP,
                      "Time series result list non-NULL after clear." );

    if ( analy->abscissa != NULL )
    {
        cleanse_result( analy->abscissa );
        analy->abscissa = NULL;
    }
    destroy_time_series( &analy->times );

    free_static_strain_data();

    /* GUI resources...*/
    if ( analy->component_menu_specs != NULL )
        free( analy->component_menu_specs );
}


/************************************************************
 * TAG( load_analysis )
 *
 * Load in a new analysis from the specified plotfile.
 */
Bool_type
load_analysis( char *fname, Analysis *analy, Bool_type reload )
{
    int fid=0;
    Database_type_griz db_type;
    Bool_type status;
    Analysis temp_analy;
    char comment[512];

    /* We are either loading another plotfile or reloading the current one.  So if a current
 *     grizhist file is open close and delete if before creating another one */
    if(analy->p_histfile)
    {
        strcpy(comment, "rm ");
        strcat(comment, analy->hist_fname);
        fclose(analy->p_histfile);
        analy->p_histfile = NULL;
        analy->hist_fname[0] = '\0';
        system(comment);
    }

    if ( !is_known_db( fname, &db_type ) )
    {
        popup_dialog( INFO_POPUP, "Unable to open file %s", fname );
        return FALSE;
    }

#ifdef SERIAL_BATCH
#else
    set_alt_cursor( CURSOR_EXCHANGE );
#endif /* SERIAL_BATCH */

    /* Reset render mode before closing old db so GUI state will be correct. */
    manage_render_mode( RENDER_MESH_VIEW, analy, TRUE );

    /* Call close_analysis() before db_close() to permit db queries. */

    close_analysis( analy );
    analy->db_close( analy );

    memset( analy, 0, sizeof( Analysis ) );

    strcpy( env.plotfile_name, fname );

    status = open_analysis( fname, analy, reload, FALSE );
    if ( !status )
        return FALSE;

    model_history_log_clear( analy );

    check_for_free_nodes( analy );

#ifdef SERIAL_BATCH
#else
    regenerate_result_menus();
#endif

    reset_mesh_window( analy );

#ifdef SERIAL_BATCH
#else
    reset_window_titles();
#endif /* SERIAL_BATCH */

    wrt_standard_db_text( analy, TRUE );

#ifdef SERIAL_BATCH
#else
    init_btn_pick_classes();
    regenerate_pick_menus();
    unset_alt_cursor();
#endif /* SERIAL_BATCH */

    return TRUE;
}


/************************************************************
 * TAG( open_history_file )
 *
 * Open a file for saving the command history.
 */
void
open_history_file( char *fname )
{
    char hist_fname[256];
    strcpy(hist_fname, env.plotfile_name);
    strcat(hist_fname, ".grizhist");
    /*Close file if we had one open already. */
    close_history_file();

    /* make sure fname is not the same as the default history file  */
    if( strcmp(fname, hist_fname) == 0 )
    {
        popup_dialog( INFO_POPUP, "Use a different file name than %s for logging history.", fname);
        return;
    }

    if ( ( env.history_file = fopen(fname, "w") ) == NULL )
    {
        popup_dialog( INFO_POPUP, "Unable to open file %s", fname );
        return;
    }

    env.save_history = TRUE;
}


/************************************************************
 * TAG( close_history_file )
 *
 * Close the file for saving the command history.
 */
void
close_history_file( void )
{
    if ( env.save_history )
        fclose( env.history_file );

    env.save_history = FALSE;

}


/************************************************************
 * TAG( history_command )
 *
 * Save a command to the command history file.
 */
void
history_command( char *command_str )
{
    Analysis *p_analy;

    /* Keep a running history commands - but only commands enter via the CLI */

    if ( !history_inputCB_cmd )
    {
        if ( history_log_index==MAXHIST )
            history_log_index=0;
        if ( history_log[history_log_index]!=NULL )
            free(history_log[history_log_index]);
        history_log[history_log_index++] = (char *) strdup(command_str);

        p_analy = get_analy_ptr();
    }

    if ( env.save_history )
    {
        fprintf( env.history_file, "%s\n", command_str );
        fflush( env.history_file );
    }
}


/************************************************************
 * TAG( history_log_command )
 *
 * Save the running history log to a file.
 */
void
history_log_command( char *fname )
{
    int i;
    FILE *fp;

    /* Keep a running history commands */

    if ( ( fp = fopen(fname, "w") ) == NULL )
    {
        popup_dialog( INFO_POPUP, "Unable to open file %s", fname );
        return;
    }

    /* Since the log array is periodic (wraps around) start with the
     * oldest commands first.
     */
    for ( i=history_log_index+1;
            i<MAXHIST;
            i++)
        if ( history_log[i]!=NULL )
            fprintf( fp, "%s\n", history_log[i] );

    for ( i=0;
            i<history_log_index;
            i++)
        if ( history_log[i]!=NULL )
            fprintf( fp, "%s\n", history_log[i] );

    fclose ( fp );
}


/************************************************************
 * TAG( history_log_clear )
 *
 * Clear the running history log.
 */
void
history_log_clear( )
{
    int i;

    for ( i=0;
            i<MAXHIST;
            i++ )
    {
        if ( history_log[i]!=NULL )
            free(history_log[i]);
        history_log[i] = (char *) NULL;
    }
}

/************************************************************
 * TAG( model_history_log_clear )
 *
 * Clear the running model state history,
 */
void
model_history_log_clear ( Analysis *analy )
{
    int i;

    for ( i=0;
            i<MAXHIST;
            i++ )
    {
        if ( model_history_log[i]!=NULL )
            free(model_history_log[i]);
        model_history_log[i] = (char *) NULL;
    }
    model_history_log_index = 0;
    analy->p_histfile = NULL;
}


/************************************************************
 * TAG( model_history_log_run)
 *
 * Executes all commands in the model state history,
 */
void
model_history_log_run( Analysis *analy )
{
    int i;

    for ( i=0;
            i<model_history_log_index;
            i++ )
    {
        if ( model_history_log[i] )
            parse_command(  model_history_log[i], analy );
    }
}

/***********************************************************
 * TAG(model_history_log_comment)
 *
 * Adds a comment to the default history file
*/
void
model_history_log_comment(char *comment, Analysis *analy)
{
    if(env.model_history_logging)
    {
        fprintf(analy->p_histfile, comment);
    }

}

/************************************************************
 * TAG( model_history_log_update)
 *
 * Add the next command to the buffer and write to the log
 * file.
 */
void
model_history_log_update( char *command, Analysis *analy )
{
    int i;
    char *first_token;
    char *copy_command;
    char *comment;
    char timestamp[64];
    char timestr[64];
    char hist_fname[256];
    char header[] = "# This is the history file for debugging purposes only\
  \n# and is for use by the griz team. Do not modify or alter in any way.\n";
    int cmdlen;
    cmdlen = strlen(command);

    copy_command = (char *)malloc((cmdlen+1)*sizeof(char));
    comment = (char *)malloc((cmdlen+20)*sizeof(char));
    time_t curtime;
    struct tm *timeinfo=NULL;

    if ( model_loading_phase ) /* We do not want to log commands from the log data */
        return;

    strcpy( copy_command, command );

    /*first_token = strtok( copy_command, "\t " );*/

    if ( history_log_index>=MAXHIST )
        history_log_index=0;

    model_history_log[model_history_log_index++] = strdup( command );
    if ( model_history_log_index>= MAXHIST )
        model_history_log_index = 0;

    if ( env.model_history_logging )
    {
        /*
         * Write timestamped command to model history file.
         */

        if ( !analy->p_histfile )
        {
            time(&curtime);
            timeinfo = localtime(&curtime);
            sprintf(timestr, "%s", asctime(timeinfo));
            strcpy(timestamp, &timestr[4]);
            strcpy(timestr, timestamp);
            /* Remove ending NL */
            timestr[strlen(timestr)-1] = '\0';
            sprintf(timestamp, "[%s]", timestr);
            strcpy( hist_fname, analy->root_name );
            strcat( hist_fname, ".grizhist" );
            analy->p_histfile = fopen( hist_fname, "at");
            strcpy(analy->hist_fname, hist_fname);
            fprintf(analy->p_histfile, "#");
            fprintf(analy->p_histfile, "%s\n", timestamp);
        }

        first_token = strtok( copy_command, "\t " );
        if((strcmp(first_token, "rdhis") != 0) &&
                (strcmp(first_token, "rdhist") != 0) &&
                (strcmp(first_token, "h") != 0) &&
                (strcmp(first_token, "hinclude") != 0))
        {
            strcpy( copy_command, command );
            fprintf(analy->p_histfile, "%s\n", copy_command );
            fflush( analy->p_histfile );
        }
        else
        {
            strcpy(copy_command, command );
            sprintf(comment, "# end %s\n", copy_command);
            model_history_log_comment(comment, analy);
        }

    }

    if(copy_command)
    {
      free(copy_command);
    }
    if(comment)
    {
      free(comment);
    }
}

/************************************************************
 * TAG( read_history_file )
 *
 * Read in a command history file and execute the commands in it.
 */
Bool_type
read_history_file( char *fname, int line_num, int loop_count,
                   Analysis *analy )
{
    FILE *infile;
    char c, str[2000], tmp_str[2000];
    int i;
    int current_line=0;

    if ( (infile = fopen(fname, "r") ) == NULL )
    {
        popup_dialog( INFO_POPUP, "Unable to open file %s", fname );
        return FALSE;
    }

    env.history_input_active = TRUE;

    if ( loop_count<= 0 )
        loop_count = 1;

    for ( i=0;
            i<loop_count;
            i++ )
    {
        current_line = 0;
        analy->hist_line_cnt = line_num;
        while ( fgets( str, 2000, infile ) != NULL &&
                !feof(infile))
        {
            /* Skip to specified line number */

            current_line++;
            if ( line_num>1 && (current_line < line_num) )
                continue;

            if ( str[0] != '#' && str[0] != '\0' )
            {
                analy->hist_paused = FALSE;
                parse_command( str, analy );

#ifndef SERIAL_BATCH
                /* If we are running interactive display this command in the
                 * history window as hilited text.
                 */
                if ( analy->echocmd )
                {
                    sprintf(tmp_str, "  [%s- Line:%d] %s", fname, analy->hist_line_cnt, str);
                    write_history_text( tmp_str, TRUE );
                }

                /* Check if user has a 'pause' in the history input */

                if ( analy->hist_paused )
                {
                    analy->hist_line_cnt++;
                    fclose( infile );
                    return TRUE;
                }
#endif
            }
            analy->hist_line_cnt++;

        } /* while */
        rewind( infile );
    } /* for loop_count */

    if ( feof(infile) )
        analy->hist_line_cnt = 0;

    fclose( infile );

    env.history_input_active = FALSE;

    return TRUE;
}


#ifdef SERIAL_BATCH

/************************************************************
 * TAG( process_serial_batch_mode )
 *
 * Process GRIZ in serial, batch mode:
 *
 *     perform selected GRIZ initialization(s), as required;
 *     establish OSMesa offscreen rendering context;
 *     read GRIZ command input file for batch operations
 */
#define MAX_STRING_LENGTH 200

static void
process_serial_batch_mode( char *batch_input_file_name, Analysis *analy )
{
    FILE
    *fp_batch_input_file;

    char
    c,
    command_string[MAX_STRING_LENGTH];

    int i;


    /* */

    if ( NULL == (fp_batch_input_file = fopen( batch_input_file_name, "r" )) )
        popup_fatal( "Unable to open batch command file" );

    init_mesh_window( analy );


    /*
     * display initial state
     */

    analy->update_display( analy );


    /*
     * process serial batch command file
     *
     * NOTE:  "quit" ends GRIZ processing
     */

    while ( !feof( fp_batch_input_file ) )
    {
        i = 0;

        c = getc( fp_batch_input_file );

        while ( c != '\n' && !feof( fp_batch_input_file ) && i < MAX_STRING_LENGTH - 1 )
        {
            command_string[i++] = c;
            c = getc( fp_batch_input_file );
        }

        command_string[i] = '\0';


        if ( command_string[0] != '#' && command_string[0] != '\0' )
        {
            if ( ( 0 != strcmp( command_string, "quit" ) ) &&
                    ( 0 != strcmp( command_string, "exit" ) ) &&
                    ( 0 != strcmp( command_string, "end"  ) ) )
                parse_command( command_string, analy );
            else
            {
                fclose( fp_batch_input_file );

                return;
            }
        }
    }

    /*
     * EOF condition
     */

    fclose( fp_batch_input_file );
}

#endif /* SERIAL_BATCH */


/************************************************************
 * TAG( usage )
 *
 * Write out GRIZ command-line syntax.
 */
static void
usage( void )
{
    static char *usage_text[] =
    {
        "\nUsage:  griz4s -i input_base_name\n",
        "            [-s]                          # single-buffer mode         #\n",
        "            [-f]                          # run in foreground          #\n",
        "            [-v]                          # size image for video       #\n",
        "            [-V]                          # show program version       #\n",
        "            [-w <width_pix> <height_pix>] # set exact image size       #\n",
        "            [-u]                          # util panel in ctrl win     #\n\n",
        "            [-beta | -alpha]              # run a alpha or beta version of Griz #\n",
        "            -version aaa                  # run a specific version of Griz #\n",
        "                                          #   aaa to grizx (-version grizex) runs the latest pre-release of Griz #\n",
        "            [-tv ]                        # run Griz under the Totalview debugger #\n",
        "            [-win32]                      # displaying Griz on Win32 platform #\n\n",
        "            [-gid <1-10>]                 # griz session id number       #\n",
        "            [-man]                        # display Griz online manual #\n\n"
        "            [-nodialog]                   # disable popup info/warning messages #\n\n"
        "            [-bname] name                 # override banner filename with new name #\n\n"
        "            [-q]                          # run in quiet - minimal output to screen (batch only)#\n",
        "            [-checkresults] [-cr]         # checks all results for corrupt numbers (Nan)#\n\n"
    };

    static char *usage_text_batch[] =
    {
        "\nUsage (Griz Batch):  griz4s -i input_base_name -b input_file_name\n",
        "            [-s]                          # single-buffer mode         #\n",
        "            [-f]                          # run in foreground          #\n",
        "            [-v]                          # size image for video       #\n",
        "            [-V]                          # show program version       #\n",
        "            [-q]                          # run in quiet - minimal output to screen #\n",
        "            [-w <width_pix> <height_pix>] # set exact image size       #\n",
        "            [-beta | -alpha]              # run a alpha or beta version of Griz #\n",
        "            -version aaa                  # run a specific version of Griz #\n",
        "                                          #   aaa to Grizx (-version grizex) runs the latest pre-release of Griz #\n",
        "            [-tv ]                        # run Griz under the Totalview debugger #\n",
        "            -b | -batch input_file_name # batch mode               #\n\n"
        "            [-bname]                      # override banner filename with new name #\n\n"
        "            [-checkresults]               # checks all results for corrupt numbers #\n\n"
    };
    int i, line_cnt=0, line_cnt_batch=0;

#ifdef SERIAL_BATCH
    line_cnt_batch = sizeof( usage_text_batch ) / sizeof( usage_text_batch[0] );
    for ( i = 0; i < line_cnt_batch; i++ )
        wrt_text( usage_text_batch[i] );
#else
    line_cnt = sizeof( usage_text ) / sizeof( usage_text[0] );
    for ( i = 0; i < line_cnt; i++ )
        wrt_text( usage_text[i] );
#endif

    return;
}

/*****************************************************************
 * TAG( update_session )
 *
 * This function will initialize a session show_colordata structure.
 */
void update_session( int operation, void *fp, int cnt )
{
    int var_id=0;
    return;

    SESSION_VAR( operation, fp, cnt, var_id++, reference_state,       G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, normals_constant,      G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, node_norm,             G_FLOAT, 3, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, vis_mat_bbox,          G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, keep_max_bbox_extent,  G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, limit_rotations,       G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, show_background_image, G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, refresh,               G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, normals_smooth,        G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, hex_overlap,           G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, interp_mode,           G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, show_bbox,             G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, show_coord,            G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, show_time,             G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, show_title,            G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, show_title_path,       G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, show_safe,             G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, show_colormap,         G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, show_colorscale,       G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, show_minmax,           G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, show_scale,            G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, show_datetime,         G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, show_tinfo,            G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, use_colormap_pos,      G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, hidden_line_width,     G_FLOAT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, float_frac_size,       G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, auto_frac_size,        G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, colormap_pos,          G_FLOAT, 4, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, mouse_mode,            G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, hilite_label,          G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, hilite_num,            G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, center_node,           G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, view_center,           G_FLOAT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, reflect,               G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, refl_orig_only,        G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, manual_backface_cull,  G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, displace_exag,         G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, displace_scale,        G_FLOAT, 3, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, show_plot_grid,        G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, show_plot_glyphs,      G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, show_plot_coords,      G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, show_legend_lines,     G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, color_plots,           G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, color_glyphs,          G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, minmax_loc,            G_CHAR, 2, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, th_smooth,             G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, th_filter_width,       G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, th_filter_type,        G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, zbias_beams,           G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, beam_zbias,            G_FLOAT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, z_buffer_lines,        G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, show_edges,            G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, hide_edges_by_mat,     G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, edge_width,            G_FLOAT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, edge_zbias,            G_FLOAT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, vec_col_set,           G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, vec_hd_col_set,        G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, vec_min_mag,           G_FLOAT8, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, vec_max_mag,           G_FLOAT8, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, vec_scale,             G_FLOAT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, vec_head_scale,        G_FLOAT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, have_grid_points,      G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, vectors_at_nodes,      G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, scale_vec_by_result,   G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, dir_vec,               G_FLOAT, 3, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, do_tensor_transform,   G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, trace_width,           G_FLOAT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, ptrace_limit,          G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, trace_disable_qty,     G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, perform_unit_conversion, G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, conversion_scale,      G_FLOAT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, conversion_offset,     G_FLOAT, 1, MODEL_VAR );

    SESSION_VAR( operation, fp, cnt, var_id++, auto_img,              G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, autoimg_format,        G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, rgb_root,              G_STRING, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, img_root,              G_STRING, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, loc_ref,               G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, show_num,              G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, show_particle_class,   G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, logscale,              G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, damage_hide,           G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, reset_damage_hide,     G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, particle_nodes_hide_background, G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, free_nodes_scale_factor,        G_FLOAT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, free_nodes_sphere_res_factor,   G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, free_nodes_mass_scaling,        G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, free_nodes_vol_scaling,         G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, free_nodes_cut_width,           G_FLOAT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, fn_output_momentum,             G_INT, 1, MODEL_VAR );

    SESSION_VAR( operation, fp, cnt, var_id++, draw_wireframe,        G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, draw_wireframe_trans,  G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, enable_brick,          G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, hide_brick,            G_INT, 1, MODEL_VAR );
    SESSION_VAR( operation, fp, cnt, var_id++, material_greyscale,    G_INT, 1, MODEL_VAR );
}

/*****************************************************************
 * TAG( process_update_session )
 *
 * This function will initialize a session data structure.
 */
void process_update_session( int operation, void *ptr, int cnt,
                             int var_id, char *var_name,
                             void *p_analy_var, void *p_session_var,
                             int type, int len, Bool_type local_var,
                             short *var_update )
{
    int    *analy_int_var_ptr;
    float  *analy_float_var_ptr;
    double *analy_dbl_var_ptr;
    char   *analy_char_var_ptr;

    int    *session_int_var_ptr;
    float  *session_float_var_ptr;
    double *session_dbl_var_ptr;
    char   *session_char_var_ptr;

    FILE *fp=NULL;
    char **session_buff=NULL;
    int    i;

    switch ( type )
    {
    case ( G_INT ):
        analy_int_var_ptr   = (int *) p_analy_var ;
        session_int_var_ptr = (int *) p_session_var ;
        break;

    case ( G_FLOAT ):
        analy_float_var_ptr   = (float *) p_analy_var ;
        session_float_var_ptr = (float *) p_session_var ;
        break;

    case ( G_FLOAT8 ):
        analy_dbl_var_ptr   = (double *) p_analy_var ;
        session_dbl_var_ptr = (double *) p_session_var ;
        break;

    case ( G_STRING ):
        analy_char_var_ptr   = (char *) p_analy_var ;
        session_char_var_ptr = (char *) p_session_var ;
        break;
    }

    switch ( operation )
    {
    case ( GET ):
        switch ( type )
        {
        case ( G_INT ):
            for ( i=0;
                    i<len;
                    i++ )
                session_int_var_ptr[i] = analy_int_var_ptr[i];
            break;
        case ( G_FLOAT ):
            for ( i=0;
                    i<len;
                    i++ )
                session_float_var_ptr[i] = analy_float_var_ptr[i];
            break;
        case ( G_FLOAT8 ):
            for ( i=0;
                    i<len;
                    i++ )
                session_dbl_var_ptr[i] = analy_dbl_var_ptr[i];
            break;
        case ( G_STRING ):
            strcpy( session_char_var_ptr, analy_char_var_ptr );
            break;
        }

        break;

    case ( PUT ):
        switch ( type )
        {
        case ( G_INT ):
            for ( i=0;
                    i<len;
                    i++ )
                analy_int_var_ptr[i] = session_int_var_ptr[i];
            break;
        case ( G_FLOAT ):
            for ( i=0;
                    i<len;
                    i++ )
                analy_float_var_ptr[i] = session_float_var_ptr[i];
            break;
        case ( G_FLOAT8 ):
            for ( i=0;
                    i<len;
                    i++ )
                analy_dbl_var_ptr[i] = session_dbl_var_ptr[i];
            break;
        case ( G_STRING ):
            strcpy( analy_char_var_ptr, session_char_var_ptr );
            break;
        }

        break;

    case ( READ ):
        if ( ptr==NULL ) return;

        break;

    case ( WRITE ):
        if ( ptr==NULL ) return;
        fp = (FILE *) ptr;

        fprintf( fp, "var_name= %s len= %d val= ", var_name, len );
        switch ( type )
        {
        case ( G_INT ):
            for ( i=0;
                    i<len;
                    i++ )
                fprintf( fp, " %d ", analy_int_var_ptr[i] );

            break;
        case ( G_FLOAT ):
            for ( i=0;
                    i<len;
                    i++ )
                fprintf( fp, " %e ", analy_float_var_ptr[i] );
            break;
        case ( G_FLOAT8 ):
            for ( i=0;
                    i<len;
                    i++ )
                fprintf( fp, " %e ", analy_dbl_var_ptr[i] );
            break;
        case ( G_STRING ):
            strcpy( analy_char_var_ptr, session_char_var_ptr );
            break;
        }
        fprintf( fp, "\n" );
        break;

    }
    analy_int_var_ptr = (int *) p_analy_var;
}


/*****************************************************************
 * TAG( get_analy_ptr )
 *
 * Returns pointer to the analysis data structure
 */
Analysis *
get_analy_ptr( )
{
    return ((Analysis *) analy_ptr );
}
