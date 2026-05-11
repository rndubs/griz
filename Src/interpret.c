/*
 * interpret.c - Command line interpreter for graphics viewer.
 *
 *  Donald J. Dovey
 *  Lawrence Livermore National Laboratory
 *  Jan 2 1992
 *
 ************************************************************************
 *
 * Modifications:
 *  I. R. Corey - Sept 15, 2004: Added new option "damage_hide" which
 *   controls if damaged elements are displayed.
 *
 *  I. R. Corey - Nov 15, 2004: Added new option "extreme_min" and
 *   "extreme_max" to display element min and maxes over all time
 *   steps. See SRC# 292.
 *
 *  I. R. Corey - Dec  28, 2004: Added new function draw_free_nodes. This
 *                function is invoked using the on free_nodes command.
 *                See SRC#: 286
 *
 *  I. R. Corey - Jan 24, 2005: Added a new free_node option to disable
 *                mass scaling of the nodes.
 *
 *  I. R. Corey - Feb 10, 2005: Added a new free_node option to enable
 *                volume disable scaling of the nodes.
 *
 *  I. R. Corey - Mar 29, 2005: Added range selection option to scale
 *                command like we now have for materials -
 *                    example: select brick 100-110
 *                See SCR#: 308
 *
 *  I. R. Corey - Apr 05, 2005: Added function to check for valid
 *                result. Called before performing various operations
 *                such as plotting (timehistory).
 *                See SCR#: 310
 *
 *  I. R. Corey - Sept 08, 2005: Alias outjpeg with outjpg.
 *                See SCR#: 348
 *
 *  I. R. Corey - Feb 02, 2006: Add a new capability to display meshless,
 *                particle-based results.
 *                See SRC# 367.
 *
 *  I. R. Corey - Mar 30, 2006: Add the capability to put multiple commands
 *                on a single line. Fucntion parse_command will be replaced
 *                by parse_single_command, and parse_command sill now strip
 *                out single commands from the input stream.
 *                See SRC# 381.
 *
 *  I. R. Corey - Oct 26, 2006: Added a new feature for enable/disable
 *                vis/invis that allows selection of an object type and
 *                material for that object.
 *                See SRC# 421.
 *
 *  I. R. Corey - Nov 09, 2006: Added a new feature for enable/disable
 *                display updates during resize events.
 *                See SRC# 421.
 *
 *  I. R. Corey - Feb 12, 2007: Added wireframe viewing capability and added
 *                logic to fix degenerate polygon faces.
 *                See SRC#437.
 *
 *  I. R. Corey - Feb 20, 2007: Added refinement to objects selection for
 *                bricks, truss, shells, and beams that allows hide/enable
 *                of indivisual elements or a range of elements.
 *                See SRC#437.
 *
 *  I. R. Corey - March 12, 2007: Added two new zoom commands: zoom forward
 *                (zf) and zoom backward (zb). Each requires a number between
 *                0 and 1 which is multiplied by the scale factor.
 *                See SRC#438.
 *
 *  I. R. Corey - Jul 25, 2007: Added feature to plot disabled materials in
 *                greyscale.
 *                See SRC#476.
 *
 *  I. R. Corey - Aug 09, 2007:	Added date/time string to render window.
 *                See SRC#478.
 *
 *  I. R. Corey - Aug 24, 2007:	Added a new help button to display cur-
 *                rent release notes.
 *                See SRC#483.
 *
 *  I. R. Corey - Oct 05, 2007:	Added new error indicator result option.
 *                See SRC#483.
 *
 *  I. R. Corey - Nov 08, 2007:	Add Node/Element labeling.
 *                See SRC#418 (Mili) and 504 (Griz)
 *
 *  I. R. Corey - Apr 18, 2008:	Allow for longer commands (more tokens).
 *                Change from max of 20 -> 1000.
 *                See SRC#529
 *
 *  I. R. Corey - May 5, 2008: Added support for code usage tracking using
 *                the AX tracker tool.
 *                See SRC#533.
 *
 *  I. R. Corey - Dec 2, 2008: Increased fracsize max from 6->10.
 *                See SRC#555
 *
 *  I. R. Corey - Jan 5, 2009: Added another form of cutplane - cut relative
 *                plane or cutrpln. With this form of the command the x,y.z
 *                values are not positions but fractions from 0-1 in each
 *                direction, for example cutrpln .5 .5 .5 0 0 1 would
 *                place the point in the center of the model.
 *                See SRC#558
 *
 *  I. R. Corey - June 16, 2009: Add exclusion capability for nodes.
 *                See SRC#539
 *
 *  I. R. Corey - Nov 09, 2009: Added enhancements to better support
 *                running multiple sessions of Griz.
 *                See SRC#638.
 *
 *  I. R. Corey - Nov 12, 2009: Fixed error in setting mo_class pointer
 *                for Beams in process_node_selection().
 *                See SRC#639.
 *
 *  I. R. Corey - Dec 1, 2009: Added a new command to clear all selects
 *                and hilites -> 'clrallpicks' or aka 'cap'.
 *                See SRC#643.
 *
 *  I. R. Corey - Dec 28, 2009: Fixed several problems releated to drawing
 *                ML particles.
 *                See SRC#648
 *
 *  I. R. Corey - Dec 28, 2010: Fixed several problems releated to drawing
 *                ML particles.
 *                See SRC#648
 *
 *  I. R. Corey - Feb 16, 2011: Added all-but (allb) option to all material
 *                selection commands.
 *                See SRC#648
 *
 *  I. R. Corey - March 24th, 2011: Added option to allow viewing of
 *                inactive elements - "on showde" -> show deleted elements
 *                and "on showode" -> show only deleted elements.
 *                See TeamForge#
 *
 *  I. R. Corey - March 28th, 2011: Fixed a bug with the alias command. It
 *                not working correctly when the aliased command had > 1
 *                keyword.
 *                See TeamForge#14543
 *
 *  I. R. Corey - January 11th, 2012: Fixed a bug with selecting ranges of
 *                elements. Id was being tested and not the label.
 *                See TeamForge#17213
 *
 *  I. R. Corey - March 30th, 2012: Added capability to set up to 10 paths
 *                for load commands.
 *                See TeamForge#17794
 *
 *  I. R. Corey - April 8th, 2012: Completed development of surface class
 *                based on new requirements.
 *                See TeamForge#17795
 *
 *  I. R. Corey - April 20th, 2012: Added ability to select objects for
 *                only visible materials - "on som".
 *                See TeamForge#17939
 *
 *  I. R. Corey - May 8th, 2012: Added ability to hide edges for invisible
 *                or excluded materials.
 *                See TeamForge#17940
 *
 *  I. R. Corey - June 25th, 2012: Added xpdf reader as default with
 *                2nd reader set to acroread if xpdf not available.
 *                See TeamForge#18256
 *
 *  I. R. Corey - June 26th, 2012: Added flag to disable echo of commands
 *                in GUI - 'echocmd'. Default value is ON.
 *                See TeamForge#18255
 *
 *  I. R. Corey - October 8th, 2012: Added new command to dump current result
 *                to a text file: 'dumpresult'.
 *                See TeamForge#18363
 *
 *  I. R. Corey - October 12th, 2012: Added capability to show/hide elements
 *                by class name.
 *                See TeamForge#19748
 *
 *  I. R. Corey - November 21th, 2012: Added capability to have a multi-command
 *                alias such as: alias new on all;l.
 *                See TeamForge#19750
 *
 *  I. R. Corey - November 25th, 2012: Set max fracsz to 12.
 *                See TeamForge#19765
 *
 *  I. R. Corey - Dec 4th, 2012: Fixed problem with dump_result and added
 *                option to choose a filename.
 *                See TeamForge#19764
 *
 *  I. R. Corey - Dec 19th, 2012: Fixed problem with updating edges when
 *                on edges command input.
 *
 *  I. R. Corey - Jan 2nd, 2013: Added flag to force single column output
 *                for TH output.
 *                See TeamForge#19785
 *
 *  I. R. Corey - February 11th, 2013: Added a flag to disable volume
 *                averaging for hex interp results.
 *                See TeamForge#
 *
 *  I. R. Corey - March 1st, 2013: Do not update display for sw commands
 *                hilite and select.
 *                See TeamForge#19547
 *
 *  I. R. Corey - March 18th, 2013: New commands to set controls for
 *                element sets.
 *                See TeamForge#19547
 *
 *************************************************************************
 */

/* #define DEBUG_SELECT 1 */
#include "griz_config.h"
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <math.h>
#include "viewer.h"
#include "draw.h"
#include "mdg.h"
#include "traction_force.h"
#include "misc.h"
#include "io_wrap.h"
#include <errno.h>

#ifdef VIDEO_FRAMER
extern void vid_select( /* int select */ );
extern void vid_cbars();
extern int vid_vlan_init();
extern int vid_vlan_rec( /* int move_frame, int record_n_frames */ );
extern void vid_cbars();
#endif

extern void update_min_max();
extern View_win_obj *v_win;
extern float crease_threshold;
extern float explicit_threshold;
extern FILE *text_history_file;
extern Bool_type text_history_invoked;

static int mat_obj_type;

extern void define_color_properties();
extern void update_current_color_property();

extern Bool_type history_inputCB_cmd;

char newtokens[MAXTOKENS][TOKENLENGTH];
int  new_token_cnt;

int previous_show_command = 0;

#define BUFSIZE 2000
#define LASTCMD 200

char newcmd[LASTCMD];

/*****************************************************************
 * TAG( Alias_obj )
 *
 * Holds a command alias.
 */
typedef struct _Alias_obj
{
    struct _Alias_obj *next;
    struct _Alias_obj *prev;
    char alias_str[TOKENLENGTH];
    int token_cnt;
    char tokens[MAXTOKENS][TOKENLENGTH];
} Alias_obj;

static Alias_obj *alias_list = NULL;

/*****************************************************************
 * TAG( Material_property_obj )
 *
 * Holds information parsed from a material manager request.
 */
typedef struct _Material_property_obj
{
    GLfloat color_props[3];
    float gslevel;
    int *materials;
    int mtl_cnt;
    int cur_idx;
} Material_property_obj;


/*****************************************************************
 * TAG( Surface_property_obj )
 *
 * Holds information parsed from a surface manager request.
 */
typedef struct _Surface_property_obj
{
    int *surfaces;
    int surf_cnt;
    int cur_idx;
} Surface_property_obj;

#ifdef JPEG_SUPPORT
/*****************************************************************
 * TAG( JPEG global variable initialization )
 *
 */
int jpeg_quality = 75;
#endif

#ifdef PNG_SUPPORT
/*****************************************************************
 * TAG( PNG global variable initialization )
 *
 */
int png_compression_level = -1; // Z_DEFAULT_COMPRESSION from zlib
#endif

char *griz_home=NULL;

static char last_command[LASTCMD] = "\n";


int alphanum_cmp(const void* ,const void*);

/* Local routines. */
static void tokenize_line( char *buf, char tokens[MAXTOKENS][TOKENLENGTH],
                           int *token_cnt, Bool_type remove_quotes );
static void check_visualizing( Analysis *analy );
static void create_alias( int token_cnt, char tokens[MAXTOKENS][TOKENLENGTH] );
static void delete_alias( int token_cnt, char tokens[MAXTOKENS][TOKENLENGTH] );
static void list_alias( );
static void alias_substitute( char tokens[MAXTOKENS][TOKENLENGTH],
                              int *token_cnt );
static void alias_expand( char *buf, int *token_cnt );
static int parse_embedded_mtl_cmd( Analysis *analy, char tokens[][TOKENLENGTH],
                                   int cnt, Bool_type preview,
                                   GLfloat *save[MTL_PROP_QTY],
                                   Bool_type *p_renorm );
static int parse_embedded_surf_cmd( Analysis *analy, char tokens[][TOKENLENGTH],
                                    int cnt, Bool_type *p_renorm );

extern void parse_single_command( char *buf, Analysis *analy );
static int include_all_elements( Analysis *analy);

static void parse_vcent( Analysis *analy, char tokens[MAXTOKENS][TOKENLENGTH],
                         int token_cnt, Redraw_mode_type *p_redraw );
static void parse_mtl_cmd( Analysis *analy, char tokens[MAXTOKENS][TOKENLENGTH],
                           int token_cnt, Bool_type src_mtl_mgr,
                           Redraw_mode_type *p_redraw, Bool_type *p_renorm );
static void parse_mtl_range( char *token, int qty, int *mat_min, int *mat_max );
static void parse_surf_cmd( Analysis *analy, char tokens[MAXTOKENS][TOKENLENGTH],
                            int token_cnt, Bool_type src_surf_mgr,
                            Redraw_mode_type *p_redraw, Bool_type *p_renorm );
static void update_previous_prop( Material_property_type cur_property,
                                  Bool_type update[MTL_PROP_QTY],
                                  Material_property_obj *props );
static void copy_property( Material_property_type property, GLfloat *source,
                           GLfloat **dest, int mqty );
static void outvec( char outvec_file_name[TOKENLENGTH], Analysis *analy );
static void write_outvec_data( FILE *outvec_file, Analysis *analy );
static void prake( int token_cnt, char tokens[MAXTOKENS][TOKENLENGTH],
                   int *ptr_ival, float pt[], float vec[], float rgb[],
                   Analysis *analy );
static int get_prake_data( int token_count, char prake_tokens[][TOKENLENGTH],
                           int *ptr_ival, float pt[], float vec[],
                           float rgb[] );
/**static **/
int tokenize( char *ptr_input_string, char token_list[][TOKENLENGTH],
              size_t maxtoken );
static void start_text_history( char text_history_file_name[] );
static void end_text_history( void );

extern void hidden_inline( char * );

static int  check_for_result( Analysis *analy, int display_warning );
static int select_integration_pts(char [MAXTOKENS][TOKENLENGTH], int, Analysis *);
static int set_inpt(int, int, char *, int, int, Analysis *);
static void show_ipt_avail(Analysis *);
static void intpts_selected(Analysis *, int*);

int
mat_name_sub(Analysis *analy, char tokens[MAXTOKENS][TOKENLENGTH], int *token_cnt);
//void
//mat_name_sub(Analysis *analy, char *tokens[MAXTOKENS][TOKENLENGTH], int *token_cnt);

void
process_mat_obj_selection ( Analysis *analy, char tokens[MAXTOKENS][TOKENLENGTH],
                            int idx, int token_cnt, int mat_qty,
                            int elem_qty, MO_class_data *p_class,
                            unsigned char *p_elem, int  *p_hide_qty, int *p_hide_elem_qty,
                            unsigned char *p_uc,
                            unsigned char setval,
                            Bool_type vis_selected, Bool_type mat_selected);
void
process_node_selection ( Analysis *analy );
int
is_node_visible( Analysis *analy, int nd );

void hide_particles( Analysis *analy,
                     Bool_type all_flag, Bool_type invis,
                     int mat );

void disable_particles( Analysis *analy,
                        Bool_type all_flag, Bool_type disable,
                        int mat );

//this is a test
void restore_color(Analysis * analy, Bool_type use_default);

void backup_current_colors(Analysis *analy, Bool_type use_default);

Bool_type
is_elem_mat_visible( Analysis *analy, int elem_id, MO_class_data *p_class );

void
dump_selection_buff( char *buff_name, unsigned char *buff, int qty );
int get_class_select_index( Analysis *analy, char *short_name );

/* Used by other files, but not by the interpreter. */
/*****************************************************************
 * TAG( read_token )
 *
 * General purpose routine to read a token (delimited by white space)
 * from a file.  Comments begin with a '#' and extend to the end of
 * the line or are enclosed in braces.  Strings in double quotes are
 * returned as a single token.
 */
void
read_token( FILE *infile, char *token, int max_length )
{
    int i;
    char c;
    char lc_char;           /* Line comment character. */
    char encl_char[2];      /* Enclosing comment characters. */
    char quote_char;        /* Quote character for quoted strings. */

    lc_char = '#';
    encl_char[0] = '{';
    encl_char[1] = '}';
    quote_char = '"';

    /*
     * Eat up any preliminary white space.
     */
    c = getc( infile );

    while ( !feof( infile ) &&
            (isspace( c ) || c == encl_char[0] || c == lc_char) )
    {
        if ( c == lc_char )
            while ( !feof( infile ) && c != '\n' )
                c = getc( infile );
        else if ( c == encl_char[0] )
            while ( !feof( infile ) && c != encl_char[1] )
                c = getc( infile );

        c = getc( infile );
    }

    i = 0;
    if ( c == quote_char )
    {
        /* Get a quoted string. */
        c = getc( infile );
        while ( !feof( infile ) && c != quote_char && i < max_length-1 )
        {
            token[i] = c;
            i++;
            c = getc( infile );
        }
    }
    else
    {
        /* Get a normal token. */
        while ( !feof( infile ) && !isspace( c ) &&
                c != encl_char[0] && c != lc_char && i < max_length-1 )
        {
            token[i] = c;
            i++;
            c = getc( infile );
        }
    }

    /*
     * Eat up comment if we ended up at one.
     */
    if ( c == lc_char )
        while ( !feof( infile ) && c != '\n' )
            c = getc( infile );
    else if ( c == encl_char[0] )
        while ( !feof( infile ) && c != encl_char[1] )
            c = getc( infile );

    token[i] = '\0';
}

/*****************************************************************
 * TAG( tokenize_line )
 *
 * Tokenize a command line.  Breaks space-separated words into
 * separate tokens.
 */
static void
tokenize_line( char *buf, char tokens[MAXTOKENS][TOKENLENGTH], int *token_cnt, Bool_type remove_quotes )
{
    int chr, word, wchr;
    char lc_char;           /* Line comment character. */
    char quote_char;        /* Quote character for quoted strings. */
    Bool_type in_quote;

    lc_char = '#';
    quote_char = '"';

    chr = 0;
    word = 0;
    wchr = 0;
    in_quote = FALSE;

    while ( isspace( buf[chr] ) )
        ++chr;

    while( buf[chr] != '\0' )
    {
        if ( in_quote )
        {
            if ( buf[chr] == quote_char )
            {
                in_quote = FALSE;

                if( !remove_quotes ){
                    tokens[word][wchr] = '"';
                    wchr++;
                }

                ++chr;
                while ( isspace( buf[chr] ) && buf[chr] != '\0' )
                    ++chr;
                /*
                 * Don't terminate token if next character is square bracket,
                 * as that would indicate we're parsing a vector or array
                 * result specification.
                 */
                if ( buf[chr] != '[' )
                {
                    tokens[word][wchr] = '\0';
                    ++word;
                    wchr = 0;
                }
            }
            else
            {
                tokens[word][wchr] = buf[chr];
                ++chr;
                ++wchr;
            }
        }
        else if ( buf[chr] == quote_char )
        {
            tokens[word][wchr] = '\0';
            if ( wchr > 0 )
                ++word;
            wchr = 0;
            if( !remove_quotes )
                tokens[word][wchr++] = '"';
            ++chr;
            in_quote = TRUE;
        }
        else if ( buf[chr] == '[' )
        {
            /* Indexing expression - pick it all off here to avoid partial
             * overlap with a quoted expression.
             */
            do
            {
                if ( buf[chr] != quote_char )
                {
                    tokens[word][wchr] = buf[chr];
                    ++wchr;
                }
                ++chr;
            }
            while ( buf[chr] != ']' && buf[chr] != '\0' );

            if ( buf[chr] == ']' )
            {
                tokens[word][wchr] = buf[chr];
                ++wchr;
                ++chr;
            }

            if ( buf[chr] == '\0' )
            {
                tokens[word][wchr] = '\0';
                ++word;
            }
        }
        else if ( buf[chr] == lc_char )
        {
            /* Comment runs to end of line. */
            break;
        }
        else if ( isspace( buf[chr] ) )
        {
            tokens[word][wchr] = '\0';
            ++word;
            wchr = 0;
            while ( isspace( buf[chr] ) && buf[chr] != '\0' )
                ++chr;
        }
        else
        {
            tokens[word][wchr] = buf[chr];
            ++chr;
            ++wchr;
            if ( buf[chr] == '\0' )
            {
                tokens[word][wchr] = '\0';
                ++word;
            }
        }
    }

    //NEWLOC

    *token_cnt = word;
}

extern Bool_type mtl_color_active;

/*****************************************************************
 * TAG( griz_set_hilite )
 *
 * Shared hilite-singleton mutation. Called by parse_command()'s
 * "hilite" branch and by the server-side pick_at handler so both
 * paths end up with identical state. id_index is the 0-based internal
 * index; label is the user-facing id (1-based unless a labels table
 * aliases it).
 */
void
griz_set_hilite( Analysis *analy, MO_class_data *p_mo_class,
                 int id_index, int label )
{
    if ( analy == NULL || p_mo_class == NULL )
        return;

    analy->hilite_class = p_mo_class;
    analy->hilite_num   = id_index;
    if ( analy->hilite_num < 0 )
    {
        analy->hilite_num   = 1;
        analy->hilite_label = 1;
    }
    else
    {
        analy->hilite_label = label;
    }
}

void
griz_clear_hilite( Analysis *analy )
{
    if ( analy == NULL )
        return;
    analy->hilite_class = NULL;
    analy->hilite_num   = 0;
    analy->hilite_label = 0;
}

/*****************************************************************
 * TAG( parse_command )
 *
 * Command parser for viewer.  Parse the commands in buf. This
 * function will strip out multiple commands from a command
 * line and pass them to parse_single_command.
 */
void
parse_command( char *input_buf, Analysis *analy )
{
    char buf[BUFSIZE], command_buf[BUFSIZE];
    int  i, j;
    int  len_buf;
    int  num_cmds = 1;
    int  next_cmd_index[1000];

    int  first_nonspace=0, start_of_cmd=0;
    int  token_cnt=0;

    len_buf = strlen(input_buf);
    if(strlen(input_buf) > BUFSIZE - 1)
    {
        popup_dialog(USAGE_POPUP, "Command size too large, bad format");
        return;
    }
    next_cmd_index[0]=0;

    history_inputCB_cmd = FALSE;

    strcpy( buf, input_buf );

    /* Commands pasted from text buffer in GUI may have preceding
     * line number in comment [] - remove chars including and
     * between [].
     */
    for ( i = 0; i < 2; i++ )
    {
        for( j = first_nonspace; j < strlen( buf ); j++ )
		{
            if (buf[j]!=' ')
            {
                first_nonspace = j;
                break;
            }
		}
        if (buf[first_nonspace]=='[')
        {
            for( j = first_nonspace; j < strlen( buf ); j++ )
			{
                if (buf[j]==']')
                {
                    first_nonspace = j+1;
                    break;
                }
			}
            for(j = first_nonspace; j < strlen( buf ); j++ )
			{
                if (buf[j]!=' ')
                {
                    first_nonspace = j;
                    break;
                }
			}
        }
        else
		{
			break;
		}
    }

    strcpy( command_buf, &buf[first_nonspace] );
    strcpy( buf,         command_buf );

    /* Include entire line for an alias command */
    if ( !strncmp("alias", buf, 5 ) )
    {
        num_cmds=1;
        next_cmd_index[0] = 0;
    }
    else
    {
        /* Need to check for alias here since they could be multi-
        * command alias.
        */
        if ( strncmp("echo", buf, 4) )
            alias_expand( buf, &token_cnt );

        len_buf = strlen( buf );
        for( i = 0; i < len_buf; i++ )
		{
            if ( buf[i] == ';' )
            {
                buf[i] = '\0';
                next_cmd_index[num_cmds++]=i+1;
            }
		}
    }

    for( i = 0; i < num_cmds; i++ )
    {
        if( strlen(&buf[next_cmd_index[i]]) > 0 )
            strcpy(command_buf, &buf[next_cmd_index[i]]);

        /* Remove file references that could be at the beginning of a command.
         * These are commands that would have come from a history file and
         * the file reference is placed in the command window.
         * If the user selects the command with the mouse and does a paste,
         * this file reference would also be copied.
         * For example:    [grizinit - Line:1] on all
         *
         * Note that the file reference in enclosed in '[ ]'.
         */

        for( j = 0; j , strlen( command_buf ); j++ )
		{
            if( command_buf[j] != ' ' )
            {
                first_nonspace = j;
                break;
            }
		}

        if (mtl_color_active)
            switch_opengl_win( MESH_VIEW );

        parse_single_command( &command_buf[start_of_cmd], analy );

        if (mtl_color_active)
            switch_opengl_win( SWATCH );
    }
}

/*****************************************************************
 * TAG( parse_single_command )
 *
 * Command parser for viewer.  Parse the single command in buf.
 */
void
parse_single_command( char *buf, Analysis *analy )
{
    char tokens[MAXTOKENS][TOKENLENGTH], tmp_tokens[MAXTOKENS][TOKENLENGTH];
    char comment[512];
    int token_cnt, token_index;
    float val, pt[3], vec[3], rgb[3], pt2[3], pt3[3], pt4[3];
    Bool_type renorm, setval, valid_command, found, unselect=FALSE;
    Redraw_mode_type redraw;
    static Bool_type cutting_plane_defined = FALSE;
    int sz[3], nodes[3];
    int i_args[4];
    int idx, ival, temp_ival, i, j, n, dim, qty;
    int start_state, stop_state, max_state, min_state;
    char result_variable[1];
    Redraw_mode_type tellmm_redraw;
    Bool_type selection_cleared;
    Bool_type old_autoselect;
    static Result result_vector[3];
    int rval;
    Htable_entry *p_hte;
    MO_class_data *p_mo_class;
    MO_class_data *p_class;
    MO_class_data **p_classes;
    Mesh_data *p_mesh;
    List_head *p_lh;
    Surface_data *p_surface;
    unsigned char *p_uc, *p_uc2;
    unsigned char *p_elem, *p_elem2;
    int elem_qty;
    int superclass;
    int qty_facets;
    int class_select_index=0;

    int *p_hide_qty, *p_hide_qty2, *p_disable_qty, p_hide_qty_temp_int=0;
    int *p_hide_elem_qty, *p_hide_elem_qty2, *p_disable_elem_qty;
    int label_num=0;
    int incr = 0;
    long pos;

    /* initialize tokens */
    for(i = 0; i < MAXTOKENS; i++)
    {
        strcpy(tokens[i], "");
    }

    int elem_sclasses[] =
    {
        G_TRUSS,
        G_BEAM,
        G_TRI,
        G_QUAD,
        G_TET,
        G_PYRAMID,
        G_WEDGE,
        G_HEX,
        G_SURFACE
    };
    Specified_obj *p_so, *p_del_so;
    Classed_list *p_cl;
    Util_panel_button_type btn_type;
    int old;
    float (*xfmat)[3];
    char *p_string;
    char *tellmm_usage = "tellmm [<result> [<first state> [<last state>]]]";
    char line_buf[2000];

    FILE *fp_outview_file;
    FILE *select_file;

    char tmp_token[256], token_upper[256];

    char pdfreader_cmd[128];

    /* Material range selection variables
     */
    int mtl, mat_qty, mat_min, mat_max;

    /* Object range selection variables
     */
    int obj, obj_min, obj_max;

    Bool_type mat_selected     = TRUE;
    Bool_type mat_brick_selected = FALSE;
    Bool_type result_selected  = TRUE;
    Bool_type vis_selected     = FALSE;
    Bool_type enable_selected  = FALSE;
    Bool_type include_selected = FALSE;
    Bool_type class_selected   = FALSE;

    int view_state; /* Used for extreme_min/max */

    float *rgb_default, *rgb_ei;

    float vert_low[3], vert_hi[3], rel_pos[3];

    char filename_with_path[MAXPATHLENGTH];

    int loop_count = 1;
    int path_index=0;
    int elem_sclass=0;
    int status=OK;

    if ( griz_home==NULL )
    {
        griz_home = getenv( "GRIZHOME" );
        if ( griz_home == NULL )
            griz_home = strdup(".");
    }

    tokenize_line( buf, tokens, &token_cnt, TRUE );

    if ( token_cnt < 1 )
        return;

    /*
     * Clean up any dialogs from the last command that the user
     * hasn't acknowledged.
     */
    clear_popup_dialogs();

    if ( strncmp("echo", tokens[0], 4) )
        alias_substitute( tokens, &token_cnt );

    /*
     * Should call getnum in slots below to make sure input is valid,
     * and prompt user if it isn't.   (Use instead of sscanf.)
     * Also getint and getstring.
     */

	//mat name substitution here
	int sucsub = mat_name_sub(analy,tokens,&token_cnt);

    redraw = NO_VISUAL_CHANGE;
    renorm = FALSE;
    valid_command = TRUE;

    if(env.history_input_active == FALSE)
    {
        if(analy->p_histfile == NULL)
        {
            analy->p_histfile = fopen(analy->hist_fname, "at");
        }
        if(analy->p_histfile != NULL)
        {
            fseek(analy->p_histfile, 0L, SEEK_CUR);
            pos = ftell(analy->p_histfile);
            model_history_log_update(buf, analy);
        }
    }


    if(sucsub){
		if ( strcmp( tokens[0], "rx" ) == 0 )
		{
			sscanf( tokens[1], "%f", &val );
			inc_mesh_rot( 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, DEG_TO_RAD(val) );
			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "ry" ) == 0 )
		{
			sscanf( tokens[1], "%f", &val );
			inc_mesh_rot( 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, DEG_TO_RAD(val) );
			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "rz" ) == 0 )
		{
			sscanf( tokens[1], "%f", &val );
			inc_mesh_rot( 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, DEG_TO_RAD(val) );
			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "tx" ) == 0 )
		{
			sscanf( tokens[1], "%f", &val );
			inc_mesh_trans( val, 0.0, 0.0 );
			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "ty" ) == 0 )
		{
			sscanf( tokens[1], "%f", &val );
			inc_mesh_trans( 0.0, val, 0.0 );
			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "tz" ) == 0 )
		{
			sscanf( tokens[1], "%f", &val );
			inc_mesh_trans( 0.0, 0.0, val );
			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "scale" ) == 0 )
		{
			sscanf( tokens[1], "%f", &val );
			set_mesh_scale( val, val, val );
			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "zf" ) == 0 ) /* Zoom forward */
		{
			get_mesh_scale( vec );
			sscanf( tokens[1], "%f", &val );

			vec[0] = val*vec[0];
			vec[1] = val*vec[1];
			vec[2] = val*vec[2];

			set_mesh_scale( vec[0], vec[1], vec[2] );
			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "zb" ) == 0 ) /* Zoom backward */
		{
			get_mesh_scale( vec );
			sscanf( tokens[1], "%f", &val );

			if ( val<0 )
				val = val*(-1.);

			vec[0] = vec[0]/val;
			vec[1] = vec[1]/val;
			vec[2] = vec[2]/val;

			set_mesh_scale( vec[0], vec[1], vec[2] );
			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "minmov" ) == 0 )
		{
			sscanf( tokens[1], "%f", &val );
			if ( val < 0.0 )
				popup_dialog( USAGE_POPUP, "minmov <pixel distance>" );
			else
				set_motion_threshold( val );
		}
		else if ( strcmp( tokens[0], "scalax" ) == 0 )
		{
			if ( token_cnt != 4 )
			{
				popup_dialog( USAGE_POPUP, "scalax <x scale> <y scale> <z scale>" );
				history_command("scalax <x scale> <y scale> <z scale>" );
				valid_command = FALSE;
			}
			else
			{
				sscanf( tokens[1], "%f", &vec[0] );
				sscanf( tokens[2], "%f", &vec[1] );
				sscanf( tokens[3], "%f", &vec[2] );
				set_mesh_scale( vec[0], vec[1], vec[2] );
				redraw = BINDING_MESH_VISUAL;
			}
		}
		else if ( strcmp( tokens[0], "r" ) == 0 )
		{
			/* Repeat the last command. */
			parse_command( last_command, analy );
		}
		else if ( strcmp( tokens[0], "hilite" ) == 0 ) /* IRC */
		{
			rval = htable_search( MESH( analy ).class_table, tokens[1], FIND_ENTRY,
								  &p_hte );

			if ( rval!=OK )
			{
				string_to_upper( tokens[1], token_upper );
				rval = htable_search( MESH( analy ).class_table, token_upper, FIND_ENTRY,
									  &p_hte );
			}
			if ( rval == OK  )
				p_mo_class = (MO_class_data *) p_hte->data;

			if ( token_cnt == 3 && is_numeric_token( tokens[2] ) )
			{
				sscanf( tokens[2], "%d", &ival );
				temp_ival = get_class_label_index( p_mo_class, ival );
				/*
				if ( p_mo_class->labels_found )

				else
					temp_ival = ival;
				*/
				if ( (ival) == analy->hilite_label
						&& p_mo_class == analy->hilite_class )
				{
					/* Hilited existing hilit object -> de-hilite. */
					griz_clear_hilite( analy );
				}
				else
				{
					griz_set_hilite( analy, p_mo_class, temp_ival, ival );
				}
				redraw = BINDING_MESH_VISUAL;
			}
			else
				popup_dialog( USAGE_POPUP, "hilite <class name> <ident>" );
		}
		else if ( strcmp( tokens[0], "clrhil" ) == 0 )
		{
			analy->hilite_class = NULL;
			redraw = BINDING_MESH_VISUAL;
		}
		else if (strcmp(tokens[0], "set_ipt") == 0 || strcmp(tokens[0], "unset_ipt") == 0 )
		{
			valid_command = select_integration_pts(tokens, token_cnt, analy);
		}
		else if(strcmp(tokens[0], "show_ipts") == 0)
		{
			show_ipt_avail(analy);
			valid_command = TRUE;
		}
		else if ( strcmp( tokens[0], "select" ) == 0 || strcmp( tokens[0], "unselect" ) == 0
				  || strcmp(tokens[0], "deselect") == 0)
		{
			if ( strcmp( tokens[0], "unselect" ) == 0 || strcmp (tokens[0], "deselect") == 0)
				unselect = TRUE;
			else
				unselect = FALSE;

			rval = htable_search( MESH( analy ).class_table, tokens[1], FIND_ENTRY,
								  &p_hte );

			if ( rval!=OK )
			{
				string_to_upper( tokens[1], token_upper );
				rval = htable_search( MESH( analy ).class_table, token_upper, FIND_ENTRY,
									  &p_hte );
			}

			if ( rval == OK && token_cnt > 2 )
			{
				p_mo_class = (MO_class_data *) p_hte->data;
				superclass = p_mo_class->superclass;

				qty = 0;
				for ( i = 2; i < token_cnt; i++ ){
					if(strcmp( tokens[i], "" ) == 0){
						continue;
					}
					/* IRC: Added March 29, 2005 */
					/* Check for a range in the object list */
					if ( is_numeric_range_token( tokens[i] ) )
					{

						parse_mtl_range(tokens[i], p_mo_class->qty, &obj_min, &obj_max ) ;
						for (obj = obj_min; obj <= obj_max; obj++ )
						{
							if( superclass != G_SURFACE )
							{
								if ( obj < 1 || obj > obj_max ||
										get_class_label_index( p_mo_class, obj ) ==
										M_INVALID_LABEL )
								{
									popup_dialog( INFO_POPUP,
												  "Invalid ident (%d) for class %s (%s); ignored.",
												  obj, p_mo_class->short_name, p_mo_class->long_name );
									continue;
								}

								/*
								 * Traverse currently selected objects to see if current
								 * candidate already exists.
								 */

								for ( p_so = analy->selected_objects; p_so != NULL;
										NEXT( p_so ) )
									if ( p_so->mo_class == p_mo_class && p_so->label == obj )
										break;

								temp_ival = get_class_label_index( p_mo_class, obj );

								if ( analy->selectonmat )
								{
									if ( !strcmp( "node", p_mo_class->short_name  ) )
									{
										if ( !is_node_visible( analy, temp_ival ) )
											continue;
									}
									else if ( !is_elem_mat_visible( analy, temp_ival, p_mo_class ) )
										continue;
								}

								/* If object exists, delete to de-select, else add. */
								if ( p_so != NULL && unselect )
								{
									DELETE( p_so, analy->selected_objects );
								}
								else
								{
									if ( !unselect && p_so == NULL)
									{
										p_so = NEW( Specified_obj, "New object selection" );
										p_so->ident = temp_ival;
										p_so->label = obj;
										p_so->mo_class = p_mo_class;
										APPEND( p_so, analy->selected_objects );
									}
								}

								qty++; /* Additions or deletions should cause redraw. */
							}
							else
							{
								p_surface = p_mo_class->objects.surfaces;
								qty_facets = p_surface->facet_qty;
								if ( obj < 1 || obj > qty_facets )
								{
									popup_dialog( INFO_POPUP,
												  "Invalid ident (%d) for class %s (%s); ignored.",
												  obj, p_mo_class->short_name, p_mo_class->long_name );
									continue;
								}

								if ( p_mo_class->labels_found )
									temp_ival = get_class_label_index( p_mo_class, obj );
								else
									temp_ival = obj;

								/*
								 * Traverse currently selected objects to see
								 * if current candidate already exists.
								 */
								for ( p_so = analy->selected_objects; p_so != NULL;
										NEXT( p_so ) )
									if ( p_so->mo_class == p_mo_class
											&& p_so->label == obj )
										break;

								/* If object exists, delete to de-select, else add. */
								if ( p_so != NULL && unselect)
								{
									DELETE( p_so, analy->selected_objects );
								}
								else
								{
									if ( !unselect && p_so == NULL)
									{
										p_so = NEW( Specified_obj, "New object selection" );
										p_so->ident    = temp_ival;
										p_so->mo_class = p_mo_class;
										p_so->label = obj;
										APPEND( p_so, analy->selected_objects );
									}
								}

								qty++; /*Additions or deletions should cause redraw.*/
							}
						}
					}
					else if ( strcmp( tokens[i], "all" ) == 0 || strcmp( tokens[i], "ALL" ) == 0 )
					{
						if( superclass != G_SURFACE )
						{
							obj_max = p_mo_class->qty;

							for( j = 1; j <= obj_max; j++ )
							{
								if ( p_mo_class->labels_found && p_mo_class->labels )
								{
									label_num = p_mo_class->labels[j-1].label_num;
									temp_ival = get_class_label_index( p_mo_class, label_num );
								}
								else
									temp_ival = j;

								if ( temp_ival == M_INVALID_LABEL )
									continue;

								if ( analy->selectonmat )
								{
									if ( !strcmp( "node", p_mo_class->short_name  ) )
									{
										if ( !is_node_visible( analy, temp_ival ) )
											continue;
									}
									else if ( !is_elem_mat_visible( analy, temp_ival, p_mo_class ) )
										continue;
								}

								/*
								 * Traverse currently selected objects to see if
								 * current candidate already exists.
								 */
								for( p_so = analy->selected_objects; p_so != NULL;
										NEXT( p_so ) )
								{
									if( p_so->mo_class == p_mo_class &&
											p_so->ident == temp_ival)
										break;
								}

								/* If object exists, delete to de-select, else add. */
								if ( p_so != NULL && unselect)
								{
									DELETE( p_so, analy->selected_objects );
								}
								else
								{
									if ( !unselect && p_so == NULL)
									{
										p_so = NEW( Specified_obj, "New object selection" );
										p_so->ident = temp_ival;
										p_so->label = label_num;
										p_so->mo_class = p_mo_class;
										APPEND( p_so, analy->selected_objects );
									}
								}

								/* Additions or deletions should cause redraw. */
								qty++;
							}
						}
						else
						{
							p_surface = p_mo_class->objects.surfaces;
							qty_facets = p_surface->facet_qty;
							for( j = 1; j <= qty_facets; j++ )
							{
								if ( p_mo_class->labels_found )
									temp_ival = get_class_label_index( p_mo_class, j );
								else
									temp_ival = j;

								if ( analy->selectonmat )
								{
									if ( !strcmp( "node", p_mo_class->short_name ) )
									{
										if ( !is_node_visible( analy, temp_ival ) )
											continue;
									}
									else if ( !is_elem_mat_visible( analy, temp_ival, p_mo_class ) )
										continue;
								}

								/*
								 * Traverse currently selected objects to see if
								 * current candidate already exists.
								 */
								for( p_so = analy->selected_objects; p_so != NULL;
										NEXT( p_so ) )
								{
									if( p_so->mo_class == p_mo_class && p_so->ident == temp_ival)
										break;
								}

								/* If object exists, delete to de-select, else add. */
								if( p_so != NULL && unselect )
								{
									DELETE( p_so, analy->selected_objects );
								}
								else
								{
									if ( !unselect && p_so == NULL)
									{
										p_so = NEW( Specified_obj, "New object selection" );
										p_so->ident    = temp_ival;
										p_so->label    = j;
										p_so->mo_class = p_mo_class;
										APPEND( p_so, analy->selected_objects );
									}
								}

								/* Additions or deletions should cause redraw. */
								qty++;
							}
						}
					}
					else
					{
						if( superclass != G_SURFACE )
						{
							j = atoi( tokens[i] );
							obj_max = p_mo_class->qty;
							if ( p_mo_class->labels )
								obj_max = p_mo_class->labels_max;

							if ( j < 1 || j > obj_max ||
									(temp_ival=get_class_label_index( p_mo_class, j )) ==
									M_INVALID_LABEL )
							{
								popup_dialog( INFO_POPUP,
											  "Invalid ident (%d) for class %s (%s); ignored.",
											  j, p_mo_class->short_name, p_mo_class->long_name );
								continue;
							}

							/*
							 * Traverse currently selected objects to see if current
							 * candidate already exists.
							 */
							for ( p_so = analy->selected_objects; p_so != NULL;
									NEXT( p_so ) )
							{
								if ( p_so->mo_class == p_mo_class &&
										(p_so->ident == temp_ival) )
									break;
							}


							/* If object exists, delete to de-select, else add. */
							/* Added && unselect to conditional to account for selecting the same obj more than once
							*                     without first deselecting it. */
							if ( p_so != NULL && unselect)
							{
								DELETE( p_so, analy->selected_objects );
							}
							else
							{
								if ( !unselect && p_so == NULL)
								{
									p_so = NEW( Specified_obj, "New object selection" );
									p_so->ident = temp_ival;
									p_so->label = j;
									p_so->mo_class = p_mo_class;
									APPEND( p_so, analy->selected_objects );
								}
							}

							qty++; /* Additions or deletions should cause redraw. */
						}
						else
						{
							p_surface = p_mo_class->objects.surfaces;
							qty_facets = p_surface->facet_qty;
							j = atoi( tokens[i] );
							if ( j < 1 || j > qty_facets )
							{
								popup_dialog( INFO_POPUP,
											  "Invalid ident (%d) for class %s (%s); ignored.",
											  j, p_mo_class->short_name, p_mo_class->long_name );
								continue;
							}

							/*
							 * Traverse currently selected objects to see if current
							 * candidate already exists.
							 */
							for ( p_so = analy->selected_objects; p_so != NULL;
									NEXT( p_so ) )
								if ( p_so->mo_class == p_mo_class &&
										(p_so->ident == j || p_so->label == j) )
									break;

							/* If object exists, delete to de-select, else add. */
							if ( p_so != NULL && unselect)
							{
								DELETE( p_so, analy->selected_objects );
							}
							else
							{
								if ( !unselect && p_so == NULL)
								{
									p_so = NEW( Specified_obj, "New object selection" );
									if ( p_mo_class->labels_found )
										p_so->ident = get_class_label_index( p_mo_class, j );
									else
										p_so->ident = j;
									p_so->label = j;
									p_so->mo_class = p_mo_class;
									APPEND( p_so, analy->selected_objects );
								}
							}

							qty++; /* Additions or deletions should cause redraw. */
						}
					}
				}

				if ( i > 2 && qty > 0 )
				{
					// Redraw if necessary
					redraw = BINDING_MESH_VISUAL;
				}
			}
			else
			{
				popup_dialog( USAGE_POPUP, "select <class name> <ident>...\n" );
				valid_command = FALSE;
			}
		}
		else if ( (strcmp( tokens[0], "clrsel" ) == 0) || (strcmp( tokens[0], "poof" ) == 0) )
		{
			selection_cleared = FALSE;

			if ( token_cnt == 1 )
			{
				if ( analy->selected_objects != NULL )
					selection_cleared = TRUE;
				DELETE_LIST( analy->selected_objects );
			}
			else
			{
				strcpy(tmp_token, tokens[1]);

				string_to_upper( tokens[1], tmp_token );
				if(!strcmp(tmp_token, "ALL"))
				{
					/* Treat this as if the user typed "clrsel" */
					if(analy->selected_objects != NULL)
						selection_cleared = TRUE;
					DELETE_LIST( analy->selected_objects);
				}

				rval = htable_search( MESH( analy ).class_table, tokens[1],
									  FIND_ENTRY, &p_hte );
				if(p_hte == NULL && !selection_cleared)
				{
					if( analy->selected_objects == NULL)
						return;
					strcpy(comment, "There is no class with a shortname of ");
					strcat(comment, tokens[1]);
					popup_dialog(WARNING_POPUP, comment);
					return;

				}
				if(p_hte != NULL)
				{
					p_mo_class = (MO_class_data *) p_hte->data;
				}

				if ( rval == OK && token_cnt == 2 && !selection_cleared)
				{
					/* Clear all for specified class. */
					p_so = analy->selected_objects;
					while ( p_so != NULL )
					{
						if ( p_so->mo_class == p_mo_class )
						{
							p_del_so = p_so;
							NEXT( p_so );
							DELETE( p_del_so, analy->selected_objects );
							selection_cleared = TRUE;
						}
						else
							NEXT( p_so );
					}
				}
				else if ( rval == OK && token_cnt > 2 )
				{
					/* Clear specific selections for class. */

					for ( i = 2; i < token_cnt; i++ )
					{
						sscanf( tokens[i], "%d", &j );
						/*j--; */
						p_so = analy->selected_objects;
						while ( p_so != NULL )
						{
							if ( p_so->mo_class == p_mo_class &&
									/*(p_so->ident == j ||*/ p_so->label == j)
							{
								DELETE( p_so, analy->selected_objects );
								selection_cleared = TRUE;
								break;
							}
							else
								NEXT( p_so );
						}
					}
				}
				else if (!selection_cleared)
				{

					popup_dialog( USAGE_POPUP,
								  "clrsel [<class name> [<ident>...]]" );
					valid_command = FALSE;
				}
			}

			if ( selection_cleared )
			{
				/**/
				/* clear_gather( analy ); */
				redraw = BINDING_MESH_VISUAL;
			}
		}
		else if ( strcmp( tokens[0], "clrallpicks" ) == 0 || strcmp( tokens[0], "cap" ) == 0 ) /* Clears all select and hilites */
		{
			parse_command( "clrsel", analy);
			parse_command( "clrhil", analy);
		}
		else if ( strcmp( tokens[0], "show" ) == 0 )
		{
            if ( analy->state_count == 0 )
            {
                // There are no states, so can't run show command
                if ( previous_show_command == 0)
                {
                    popup_dialog(INFO_POPUP, "Ignoring requests to show as there are no states");
                    previous_show_command = 1;
                }
                return;
            }

			analy->th_plotting   = FALSE;
			if (  strncmp( tokens[1], "mat", 3 ) == 0 )
				analy->result_active = FALSE;
			analy->damage_result = FALSE;
			analy->damage_hide   = FALSE;

			strcpy( analy->curr_result, tokens[1] );

			if ( strcmp( tokens[1], "damage" ) == 0 )
			{
				if ( token_cnt != 6 )
				{
					popup_dialog( USAGE_POPUP, "show damage <\"vx\" | \"vy\" | \"vz\"> <vel_cutoff> <relVol_cutoff> <eps_cutoff>");
					valid_command = FALSE;
				}
				else
				{
					if ( strncmp( tokens[2], "v", 1 ) != 0 )
					{
						popup_dialog( USAGE_POPUP, "show damage <\"vx\" | \"vy\" | \"vz\"> <vel_cutoff> <relVol_cutoff> <eps_cutoff>");
						valid_command = FALSE;
					}
					else
					{
						strcpy( analy->damage_params.vel_dir, tokens[2] );
						sscanf( tokens[3], "%f", analy->damage_params.cut_off );
						sscanf( tokens[4], "%f", analy->damage_params.cut_off+1 );
						sscanf( tokens[5], "%f", analy->damage_params.cut_off+2 );
						analy->result_mod    = TRUE;
						analy->damage_result = TRUE;
						if ( parse_show_command( tokens[1], analy ) )
						{
							parse_command( "delth damage", analy );
							// Reset result source
							env.curr_analy->result_source = env.curr_analy->prev_result_source;
							redraw = BINDING_MESH_VISUAL;
						}
						else
						{
							analy->damage_result = FALSE;
							valid_command = FALSE;
						}
					}
				}
			}
			else
			{
				analy->result_mod = TRUE;
				if ( !strcmp("show", tokens[0]) && strcmp("mat", tokens[1]) )
				{
					strcpy( analy->ei_result_name,tokens[1] );
				}
                else
                {
                    /* Command is "show mat", need to deselect any materials */
                    parse_command("clrsel mat", analy);
                }

				if ( parse_show_command( tokens[1], analy ) ){
					redraw = BINDING_MESH_VISUAL;
				}
				else
					valid_command = FALSE;
			}
			if ( valid_command )
				analy->extreme_result = FALSE;
			float st_time;
			int st = analy->state_p->state_no+1;
			rval = mc_query_family( analy->db_ident, STATE_TIME,
								   (void *) &st, NULL,
								(void *) &st_time );
			if ( rval != 0 )
			{
				mc_print_error( "parse_single_command() call "
								"mc_query_family( STATE_TIME )", rval );
				return;
			}
			if(st_time != analy->state_p->time)
			{
			   change_time( analy->state_p->time, analy);
			}
		}

		/* Multi-commands like "show" should go here for faster parsing.
		 * Follow those with display-changing commands.
		 * Follow those with miscellaneous commands (info, etc.).
		 */

		else if ( strcmp( tokens[0], "quit" ) == 0   ||
				  strcmp( tokens[0], "done" ) == 0   ||
				  strcmp( tokens[0], "exit" ) == 0   ||
				  strcmp( tokens[0], "end" ) == 0 )
		{
			write_log_message( );

	#ifdef TIME_GRIZ
			manage_timer( 8, 1 );
	#endif
			if ( analy->p_histfile )
			{
				strcpy(comment, "rm ");
				strcat(comment, analy->hist_fname);
				fclose ( analy->p_histfile );
				system(comment);
			}
			quit( 0 );
		}
		else if ( strcmp( tokens[0], "help" ) == 0 || strcmp( tokens[0], "man" ) == 0 ||  strcmp( tokens[0], "grizman" ) == 0 ||
				  strcmp( tokens[0], "?" ) == 0 )
		{
			if ( griz_home != NULL )
			{
	#ifdef MAC_OS
				sprintf( pdfreader_cmd, "open %s/griz_manual.pdf &", griz_home );
				status = system( pdfreader_cmd );
	#else
				status = system( "which xpdf" );
				if ( status == 0 )
				{
					sprintf( pdfreader_cmd, "xpdf %s/griz_manual.pdf &", griz_home );
					status = system( pdfreader_cmd );
				}
				else
				{
                    status = system( "which evince" );
                    if ( status == 0 )
                    {
                        sprintf( pdfreader_cmd, "evince %s/griz_manual.pdf &", griz_home);
                        status = system( pdfreader_cmd );
                    }
                    else
                    {
                        sprintf( pdfreader_cmd, "acroread %s/griz_manual.pdf &", griz_home );
                        status = system( pdfreader_cmd );
                        if ( status != 0 )
                        {
                            // Could not open Griz release notes with either
                            // xpdf, evince, or acroread
                            popup_dialog( INFO_POPUP,
                                          "Unable to open Griz manual" );
                        }
                    }
				}
	#endif
			}
		}
		else if ( strcmp( tokens[0], "relnotes" ) == 0 || strcmp( tokens[0], "notes" ) == 0 ||  strcmp( tokens[0], "rn" ) == 0 ||
				  strcmp( tokens[0], "rnotes" ) == 0 )
		{
			if ( griz_home != NULL )
			{
	#ifdef MAC_OS
				sprintf( pdfreader_cmd, "open %s/griz_relnotes.pdf &", griz_home );
				status = system( pdfreader_cmd );
	#else
				status = system( "which xpdf" );

				if ( status == 0 )
				{
					sprintf( pdfreader_cmd, "xpdf %s/griz_relnotes.pdf &", griz_home );
					status = system( pdfreader_cmd );
				}
				else
				{
                    status = system( "which evince" );
                    if ( status == 0 )
                    {
                        sprintf( pdfreader_cmd, "evince %s/griz_relnotes.pdf &", griz_home);
                        status = system( pdfreader_cmd );
                    }
                    else
                    {
                        sprintf( pdfreader_cmd, "acroread %s/griz_relnotes.pdf &", griz_home );
                        status = system( pdfreader_cmd );
                        if ( status != 0 )
                        {
                            // Could not open Griz release notes with either
                            // xpdf, evince, or acroread
                            popup_dialog( INFO_POPUP,
                                          "Unable to open Griz release notes" );
                        }
                    }
				}
	#endif
			}
		}
		else if ( strcmp( tokens[0], "copyrt" ) == 0 )
		{
			copyright();
		}

		/* IRC: Added May 15, 2007: New function to hide all windows except Render window */
		else if ( strcmp( tokens[0], "hidewin" ) == 0 || strcmp( tokens[0], "pushwin" ) == 0)
		{
			pushpop_window( PUSHPOP_BELOW );
		}
		else if ( strcmp( tokens[0], "unhidewin" ) == 0 || strcmp( tokens[0], "popwin" ) == 0 )
		{
			pushpop_window( PUSHPOP_ABOVE);
		}
		else if ( strcmp( tokens[0], "tell" ) == 0 )
		{
			if ( token_cnt > 1 )
				parse_tell_command( analy, tokens, token_cnt, TRUE, &redraw );
			else
				popup_dialog( USAGE_POPUP,
							  "tell <reports>\n"
							  "  Where <reports> can be any sequence of:\n"
							  "    info\n"
							  "    times [<first state> [<last state>]]\n"
							  "    results\n"
							  "    class\n"
							  "    pos <class> <ident>\n"
							  "    mm [<result> [<first state> [<last state>]]]\n"
							  "    em\n"
							  "    th\n"
							  "    iso\n"
							  "    view\n\n"
							  "  Aliases\n"
							  "    \"lts [<args>]\" for \"tell times [<args>]\"\n"
							  "    \"info\" for \"tell info class view\"\n"
							  "    \"tellmm <args>\" for \"tell mm <args>\"\n"
							  "    \"tellpos <args>\" for \"tell pos <args>\"\n"
							  "    \"telliso\" for \"tell iso\"\n"
							  "    \"tellem\" for \"tell em\"" );
		}
		else if ( strcmp( tokens[0], "info" ) == 0 )
		{
			char info_tokens[4][TOKENLENGTH] =
			{
				"tell", "info", "class", "view"
			};

			parse_tell_command( analy, info_tokens, 4, TRUE, &redraw );
		}
		else if ( strcmp( tokens[0], "lts" ) == 0 )
		{
			parse_tell_command( analy, tokens, token_cnt, FALSE, &redraw );
		}
        else if ( strcmp( tokens[0], "tellmm" ) == 0 )
		{
			parse_tell_command( analy, tokens, token_cnt, FALSE, &redraw );
		}
		else if ( strcmp( tokens[0], "tellpos" ) == 0)
		{
			parse_tell_command( analy, tokens, token_cnt, FALSE, &redraw );
		}
		else if ( strcmp( tokens[0], "savtxt" ) == 0)
		{
			if ( token_cnt == 2 )
				start_text_history( tokens[1] );
			else
				popup_dialog( USAGE_POPUP, "savtxt <filename>" );
		}
		else if ( strcmp( tokens[0], "endtxt" ) == 0)
		{
			end_text_history();
		}
		else if ( strcmp( tokens[0], "load" ) == 0 )
		{
			if ( token_cnt > 1 )
			{
				update_file_path( analy, tokens[1], filename_with_path );
				if ( !load_analysis( filename_with_path, analy, FALSE ) )
					return;
			}
			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "reload" ) == 0 )
		{
            // There is no reason to load_analysis.  
            // Really just need to get the current states.
            rval = mc_reload_states( analy->db_ident);
            
            redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "bufqty" ) == 0 )
		{

			if ( token_cnt == 3 )
			{
				p_string = tokens[1];
				j = atoi( tokens[2] );
			}
			else if ( token_cnt == 2 )
			{
				p_string = NULL;
				j = atoi( tokens[1] );
			}
			else
				valid_command = FALSE;

			if ( j < 0 )
				valid_command = FALSE;

			if ( valid_command )
				for ( i = 0; i < analy->mesh_qty; i++ )
					analy->db_set_buffer_qty( analy->db_ident, i, p_string, j );
			else
				popup_dialog( USAGE_POPUP,
							  "bufqty [<class name>] <buffer qty>" );
		}
		else if ( strcmp( tokens[0], "rview" ) == 0 )
		{
			if (analy->rb_vcent_flag)
			{
				history_command(  "vcent off" );
				parse_command( "vcent off", analy);
				parse_command( "scale 1", analy);
			}
			reset_mesh_transform();

			if (analy->rb_vcent_flag)
			{
				analy->rb_vcent_flag=FALSE;
				adjust_near_far( analy );
			}
			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "vcent" ) == 0 )
		{
			parse_vcent( analy, tokens, token_cnt, &redraw );
		}
		else if ( strcmp( tokens[0], "conv" ) == 0 )
		{
			if ( token_cnt > 3 )
				popup_dialog( USAGE_POPUP, "conv [<scale> [<offset>]]" );
			else
			{
				if ( token_cnt > 1 )
				{
					sscanf( tokens[1], "%f", &val );
					analy->conversion_scale = val;

					if ( token_cnt > 2 )
					{
						sscanf( tokens[2], "%f", &val );
						analy->conversion_offset = val;
					}
					else
						analy->conversion_offset = 0.0;
				}
				else
				{
					analy->conversion_scale = 1.0;
					analy->conversion_offset = 0.0;
				}

				if ( analy->perform_unit_conversion )
					redraw = redraw_for_render_mode( FALSE, RENDER_ANY,
													 analy );
			}
		}
		else if (strcmp( tokens[0], "clrconv" ) == 0 )
		{
			analy->conversion_scale = 1.0;
			analy->conversion_offset = 0.0;

			analy->perform_unit_conversion = FALSE;

			redraw = redraw_for_render_mode( FALSE, RENDER_ANY, analy );
		}
		else if ( strcmp( tokens[0], "res" ) == 0 ||
				  strcmp( tokens[0], "pres" ) == 0 || strcmp( tokens[0], "fres" ) == 0)
		{
			analy->free_nodes_sphere_res_factor = atoi( tokens[1] );
			if (analy->free_nodes_sphere_res_factor<2)
				analy->free_nodes_sphere_res_factor = 2;
			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "pscale" ) == 0 || strcmp( tokens[0], "fscale" ) == 0 ||
				  strcmp( tokens[0], "psize" )  == 0 || strcmp( tokens[0], "fsize" )  == 0)
		{
			analy->free_nodes_scale_factor = atof( tokens[1] );
			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "pcutwidth" ) == 0 || strcmp( tokens[0], "fcutwidth" ) == 0 ||
				  strcmp( tokens[0], "pcutw" )     == 0 || strcmp( tokens[0], "fcutw" )     == 0)
		{
			analy->free_nodes_cut_width = atof( tokens[1] );
			reset_face_visibility( analy );
			redraw = NONBINDING_MESH_VISUAL;
			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "on" ) == 0 ||
				  strcmp( tokens[0], "off" ) == 0 )
		{
			if ( strcmp( tokens[0], "on" ) == 0 )
				setval = TRUE;
			else
				setval = FALSE;

			for ( i = 1; i < token_cnt; i++ )
			{
				if ( strcmp( tokens[i], "box" ) == 0 || strcmp( tokens[i], "bbox" ) == 0 )
					analy->show_bbox = setval;
				else if ( strcmp( tokens[i], "echocmd" ) == 0 )
					analy->echocmd = setval;
				else if ( !strcmp( tokens[i], "autogray"))
				{
					if(setval)
					{
						analy->auto_gray = TRUE;
					}else
					{
						analy->auto_gray = FALSE;
					}
					if(analy->dimension == 3)
					{
						draw_grid(analy);
					} else
					{
						draw_grid_2d( analy);
					}
				}
				else if( strcmp( tokens[i], "ipt_labels") == 0)
					analy->int_point_labels = setval;
				else if ( strcmp( tokens[i], "coord" ) == 0 )
					analy->show_coord = setval;
				else if ( strcmp( tokens[i], "time" ) == 0 )
					analy->show_time = setval;
				//NEW
				else if ( strcmp( tokens[i], "snap" ) == 0 )
					analy->use_snap = setval;
				else if ( strcmp( tokens[i], "mat_labels" ) == 0 ){
					analy->mat_labels_active = setval;
					if (setval == TRUE){
						//this is for a branch test commit REMOVETHIS
						analy->sorted_labels = analy->sorted_names;
						analy->mat_labels = analy->mat_names;
						analy->mat_labels_reversed = analy->mat_names_reversed;
					}
					else{
						//this is for a branch test commit REMOVETHIS
						analy->sorted_labels = analy->sorted_nums;
						analy->mat_labels = analy->mat_nums;
						analy->mat_labels_reversed = analy->mat_nums_reversed;
					}
				}
				//END
				else if ( strcmp( tokens[i], "title" ) == 0 )
				{
					analy->show_title      = setval;
				}
				else if ( strcmp( tokens[i], "path" ) == 0 )
					analy->show_title_path = setval;
				else if ( strcmp( tokens[i], "cmap" ) == 0 )
					analy->show_colormap = setval;
				else if ( strcmp( tokens[i], "minmax" ) == 0 )
					analy->show_minmax = setval;
				else if ( strcmp( tokens[i],  "scale" )    == 0 ||
						  strncmp( tokens[i], "dscal", 5 ) == 0 )
					analy->show_scale = setval;
				else if ( strcmp( tokens[i], "date" ) == 0 )
					analy->show_datetime = setval;
				else if ( strcmp( tokens[i], "tinfo" ) == 0 )
					analy->show_tinfo = setval;
				else if ( strcmp( tokens[i], "all" ) == 0 )
				{
					analy->show_coord      = setval;
					analy->show_time       = setval;
					analy->show_title      = setval;
					analy->show_title_path = setval;
					analy->show_colormap   = setval;
					analy->show_minmax     = setval;
					analy->show_scale      = setval;
					analy->show_datetime   = setval;
					analy->show_tinfo      = setval;
					if ( !setval )
						parse_command( "off ei", analy ); /* On all should not enable ei mode */
				}
				else if ( strcmp( tokens[i], "cscale" ) == 0 )
					analy->show_colorscale = setval;
				else if ( strcmp( tokens[i], "edges" ) == 0 )
				{
					if ( strcmp( tokens[0], "on" ) == 0 )
						get_mesh_edges( analy->cur_mesh_id, analy );

					analy->show_edges = setval;
					if ( setval && MESH( analy ).edge_list == NULL )
						popup_dialog( INFO_POPUP,
									  "Empty edge list; check mat visibility" );
					update_util_panel( VIEW_EDGES, NULL );
				}
				else if ( strcmp( tokens[i], "hideedges" ) == 0 )
				{
					analy->hide_edges_by_mat = setval;
				}
				else if ( strcmp( tokens[i], "safe" ) == 0 )
					analy->show_safe = setval;
				else if ( strcmp( tokens[i], "shrfac" ) == 0 )
					analy->shared_faces = setval;
				else if ( strcmp( tokens[i], "rough" ) == 0 )
				{
					analy->show_roughcut = setval;
					reset_face_visibility( analy );
					renorm = TRUE;
				}
				else if ( strcmp( tokens[i], "cut" ) == 0 )
				{
					if ( cutting_plane_defined || analy->show_particle_cut )
					{
						analy->show_cut = setval;
						if ( setval)
							gen_cuts( analy );

	#ifdef HAVE_VECTOR_CARPETS
						gen_carpet_points( analy );
	#endif
						check_visualizing( analy );

						analy->show_particle_cut = setval;
					}
					else
					{
						if ( !cutting_plane_defined && setval )
							popup_dialog( WARNING_POPUP,
										  "Cutting plane must be defined BEFORE display/removal of the cutting plane.\n"
										  "cutpln <pt x> <pt y> <pt z> <norm x> <norm y> <norm z>" );
						analy->show_particle_cut = setval;
						renorm = TRUE;
					}
				}
				else if ( strcmp( tokens[i], "con" ) == 0 )
				{
					analy->show_contours = setval;
					gen_contours( analy );
				}
				else if ( strcmp( tokens[i], "iso" ) == 0 )
				{
					analy->show_isosurfs = setval;
					gen_isosurfs( analy );
	#ifdef HAVE_VECTOR_CARPETS
					gen_carpet_points( analy );
	#endif
					check_visualizing( analy );
				}
				else if ( strcmp( tokens[i], "vec" ) == 0 )
				{
					if ( setval &&
							analy->vector_result[0] == NULL &&
							analy->vector_result[1] == NULL &&
							analy->vector_result[2] == NULL )
					{
						popup_dialog( INFO_POPUP, "%s%s",
									  "Please set a vector result, i.e.,\n",
									  "  \"vec <vx> <vy> <vz>\"." );
						analy->show_vectors = FALSE;
					}
					else
						analy->show_vectors = setval;

					update_vec_points( analy );
					if(strcmp(tokens[0], "on") == 0)
					{
						analy->last_mesh_view_mode = analy->mesh_view_mode;
					}
					/*check_visualizing( analy ); */
					if(strcmp(tokens[0], "off") == 0)
					{
						/* restore render mode and reload the last result */
						analy->mesh_view_mode = analy->last_mesh_view_mode;
					}
					if(analy->cur_result != NULL)
					{
						load_result(analy, TRUE, TRUE, FALSE);
					}
				}
				else if ( strcmp( tokens[i], "sphere" ) == 0 )
					analy->show_vector_spheres = setval;
				else if ( strcmp( tokens[i], "sclbyres" ) == 0 )
					analy->scale_vec_by_result = setval;
				else if ( strcmp( tokens[i], "zlines" ) == 0 )
					analy->z_buffer_lines = setval;
	#ifdef HAVE_VECTOR_CARPETS
				else if ( strcmp( tokens[i], "carpet" ) == 0 )
				{
					if ( setval &&
							analy->vec_id[0] == VAL_NONE &&
							analy->vec_id[1] == VAL_NONE &&
							analy->vec_id[2] == VAL_NONE )
					{
						popup_dialog( INFO_POPUP, "%s%s",
									  "Please set a vector result, i.e.,\n",
									  "  \"vec <vx> <vy> <vz>\"." );
						analy->show_carpet = FALSE;
					}
					else
						analy->show_carpet = setval;
					gen_reg_carpet_points( analy );
					gen_carpet_points( analy );
					check_visualizing( analy );
				}
	#endif
				else if ( strcmp( tokens[i], "refresh" ) == 0 )
					analy->refresh = setval;
				else if ( strcmp( tokens[i], "sym" ) == 0 )
					analy->reflect = setval;
				else if ( strcmp( tokens[i], "cull" ) == 0 )
					analy->manual_backface_cull = setval;
				else if ( strcmp( tokens[i], "defz") == 0 )
				{
					if ( setval )
					{
						analy->z_poly_last = analy->z_poly_offset;
						analy->z_poly_offset = 0.0;
					}
					else
					{
						val = analy->z_poly_offset;
						analy->z_poly_offset = analy->z_poly_last;
						analy->z_poly_last = val;
					}
				}
				else if ( strcmp( tokens[i], "polyz") == 0 )
				{
					if ( setval )
					{
						analy->z_poly_last = analy->z_poly_offset;
						analy->z_poly_offset = -1.0;
					}
					else
					{
						val = analy->z_poly_offset;
						analy->z_poly_offset = analy->z_poly_last;
						analy->z_poly_last = val;
					}
				}
				else if ( strcmp( tokens[i], "volz") == 0 )
				{
					if ( setval )
					{
						analy->z_poly_last = analy->z_poly_offset;
						analy->z_poly_offset = 1.0;
					}
					else
					{
						val = analy->z_poly_offset;
						analy->z_poly_offset = analy->z_poly_last;
						analy->z_poly_last = val;
					}
				}
				else if ( strcmp( tokens[i], "refsrf" ) == 0 )
					analy->show_ref_polys = setval;
				else if ( strcmp( tokens[i], "refres" ) == 0 )
					analy->result_on_refs = setval;
				else if ( strcmp( tokens[i], "extsrf" ) == 0 )
					analy->show_extern_polys = setval;
				else if ( strcmp( tokens[i], "timing" ) == 0 )
					env.timing = setval;
				else if ( strcmp( tokens[i], "derivtime" ) == 0 )
					env.result_timing = setval;
				else if ( strcmp( tokens[i], "thsm" ) == 0 )
					analy->th_smooth = setval;
				else if ( strcmp( tokens[i], "conv" ) == 0 )
					analy->perform_unit_conversion = setval;
				else if ( strcmp( tokens[i], "bmbias" ) == 0 )
					analy->zbias_beams = setval;
				else if ( strcmp( tokens[i], "bbmax" ) == 0 )
					analy->keep_max_bbox_extent = setval;
				else if ( strcmp( tokens[i], "autosz" ) == 0 )
					analy->auto_frac_size = setval;
				else if ( strcmp( tokens[i], "locref" ) == 0 )
					analy->loc_ref = setval;
				else if ( strcmp( tokens[i], "autorgb" ) == 0 )
				{
					if ( setval && analy->img_root == NULL )
						popup_dialog( INFO_POPUP, "%s\n%s\n%s",
									  "Please specify an RGB root name",
									  "with \"autorgb <root name>\"",
									  "on command ignored" );
					else
						analy->auto_img = setval;
				}
				else if ( strcmp( tokens[i], "autoimg" ) == 0 )
				{
					if ( setval && analy->img_root == NULL )
						popup_dialog( INFO_POPUP, "%s\n%s",
									  "Please specify an image root name with the "
									  "\"autoimg\" command;",
									  "on command ignored" );
					else
						analy->auto_img = setval;
				}
				else if ( strcmp( tokens[i], "num" ) == 0 )
				{
					if ( analy->num_class == NULL )
						popup_dialog( USAGE_POPUP, "%s\n%s\n%s",
									  "Please specify element classes",
									  "with \"numclass <classname>...\"",
									  "on command ignored" );
					else
						analy->show_num = setval;
				}

				else if ( strcmp( tokens[i], "lighting" ) == 0 )
				{
					if ( analy->dimension == 2 && setval == TRUE )
						popup_dialog( INFO_POPUP,
									  "Lighting is not utilized for 2D meshes." );
					else
						v_win->lighting = setval;
				}
				else if ( strcmp( tokens[i], "bgimage" ) == 0 )
					analy->show_background_image = setval;
				else if ( strcmp( tokens[i], "coordxf" ) == 0 )
				{
	#ifdef HAVE_NEW_RES_ENUMS
					/*
					 * Need to re-define derived result enums to enable this specificity
					 * without having to perform string comparisons.  Also need to consider
					 * desirability and method to allow tensor transform on appropriate
					 * primal results.
					 */
					analy->do_tensor_transform = setval;
					if ( analy->tensor_transform_matrix != NULL
							&& (    analy->result_id >= VAL_SHARE_EPSX
									&& analy->result_id <= VAL_SHARE_EPSZX
									||
									analy->result_id >= VAL_SHARE_SIGX
									&& analy->result_id <= VAL_SHARE_SIGZX ) )
	#endif
						if ( analy->do_tensor_transform != setval )
						{
							analy->do_tensor_transform = setval;
							analy->result_mod = TRUE;
							if ( setval && analy->ref_frame == LOCAL )
								/* These two are exclusive of each other. */
								analy->ref_frame = GLOBAL;
						}
				}
				else if ( strcmp( tokens[i], "particles" ) == 0 )
					analy->show_particle_class = setval;
				else if ( strcmp( tokens[i], "glyphs" ) == 0 )
					analy->show_plot_glyphs = setval;
				else if ( strcmp( tokens[i], "plotcol" ) == 0 )
					analy->color_plots = setval;
				else if ( strcmp( tokens[i], "glyphcol" ) == 0 )
					analy->color_glyphs = setval;
				else if ( strcmp( tokens[i], "leglines" ) == 0 )
					analy->show_legend_lines = setval;
				else if ( strcmp( tokens[i], "plotgrid" ) == 0 )
					analy->show_plot_grid = setval;
				else if ( strcmp( tokens[i], "plotcoords" ) == 0 )
				{
					analy->show_plot_coords = setval;
					manage_plot_cursor_display( setval, analy->render_mode,
												analy->render_mode );
				}
				else if ( strcmp( tokens[i], "derived" ) == 0 )
				{
					switch ( analy->result_source )
					{
					case DERIVED:
						if ( !setval )
						{
							/* At least one table must be on. */
							popup_dialog( INFO_POPUP, "Turning on primal result table." );
							analy->result_source = PRIMAL;
						}
						break;
					case PRIMAL:
						if ( setval )
							/* From primal to both. */
							analy->result_source = ALL;
						break;
					case ALL:
						if ( !setval )
							/* From both to primal. */
							analy->result_source = PRIMAL;
						break;
					}

					analy->result_mod = refresh_shown_result( analy );
				}
				else if ( strcmp( tokens[i], "primal" ) == 0 )
				{
					switch ( analy->result_source )
					{
					case DERIVED:
						if ( setval )
							/* From derived to both. */
							analy->result_source = ALL;
						break;
					case PRIMAL:
						if ( !setval )
						{
							/* At least one table must be on. */
							popup_dialog( INFO_POPUP,
										  "Turning on derived result table." );
							analy->result_source = DERIVED;
						}
						break;
					case ALL:
						if ( !setval )
							/* From both to derived. */
							analy->result_source = DERIVED;
						break;
					}

					analy->result_mod = refresh_shown_result( analy );
				}
				else if ( strcmp( tokens[i], "hex_overlap" ) == 0 )
				{
					if ( setval != analy->hex_overlap )
					{
						analy->hex_overlap = setval;
						update_hex_adj( analy );
						reset_face_visibility( analy );
						if ( analy->dimension == 3 )
							renorm = TRUE;
						redraw = NONBINDING_MESH_VISUAL;
					}
				}
				else if ( strcmp( tokens[i], "su" ) == 0 )
				{
					env.curr_analy->suppress_updates = setval;
					suppress_display_updates( setval );
				}
				else if ( strcmp( tokens[i], "ei" ) == 0 )
				{
					if ( analy->ei_result != setval )
					{
						analy->ei_result = setval;

						if ( !setval ) /* Turing EI mode off */
						{
							redraw = BINDING_MESH_VISUAL;

							parse_command( "setcol bg 1 1 1", analy );

							strcpy(tmp_token, "show ");
							strcat(tmp_token, analy->ei_result_name);
							parse_command( tmp_token, analy );

							/* Delete the plot for this result if computing an EI result */
							strcpy(tmp_token, "delth ");
							strcat(tmp_token, analy->ei_result_name);
							parse_command( tmp_token, analy );
						}
						else /* Enabling EI */
						{
							analy->ei_result = setval;
							parse_command( "setcol bg .7568 .8039 .80391", analy );

							strcpy(tmp_token, "show ");
							strcat(tmp_token, analy->ei_result_name);
							parse_command( tmp_token, analy );
						}
					}
				}
				else if ( strcmp( tokens[i], "logscale" ) == 0 )
				{
					analy->logscale = setval;
				}
				else if ( strcmp( tokens[i], "grayscale" ) == 0  ||
						  strcmp( tokens[i], "matgs" )     == 0  ||
						  strcmp( tokens[i], "gs" ) == 0 )
				{
					analy->material_greyscale = setval;
					update_util_panel( VIEW_GS, NULL );
				}
				else if ( strcmp( tokens[i], "damage_hide" ) == 0 )
				{
					analy->damage_hide       = setval;
					analy->reset_damage_hide = TRUE;
					analy->result_mod = TRUE;
					reset_face_visibility( analy );
					if ( analy->dimension == 3 ) renorm = TRUE;
					redraw = BINDING_MESH_VISUAL;
				}
				else if ( strcmp( tokens[i], "fn_output_mom" ) == 0 )
				{
					analy->fn_output_momentum = setval;
				}
				else if( strcmp( tokens[i], "particle_results_on_facets") == 0 )
				{
					analy->render_particle_results_onto_bg_elements = setval;
				}
				else if ( strcmp( tokens[i], "free_nodes" ) == 0 ||
						  strcmp( tokens[i], "particle" )   == 0 ||
						  strcmp( tokens[i], "particles" )  == 0 ||
						  strcmp( tokens[i], "fn" )         == 0 ||
						  strcmp( tokens[i], "pn" ) == 0)
				{

					if ( strcmp( tokens[i], "particle" )  == 0 ||
							strcmp( tokens[i], "particles" ) == 0 ||
							strcmp( tokens[i], "pn" ) == 0 )
						analy->particle_nodes_enabled = setval;
					else
						analy->free_nodes_enabled = setval;

					if ( !analy->particle_nodes_found )
					{
						analy->particle_nodes_enabled = 0;
					}


					analy->result_mod = FALSE; /* IRC: was TRUE */

					qty = MESH( analy ).material_qty;

					token_index = 1;
					if ( analy->dimension == 3 ) renorm = TRUE;
					redraw = BINDING_MESH_VISUAL;

					if (!strcmp( tokens[i+token_index], "scale"))
					{
						analy->free_nodes_scale_factor = atof( tokens[i + token_index+1] );
						token_index+=2;
					}

					/* User has input the sphere resolution factor */
					if (!strcmp( tokens[i+token_index], "res"))
					{
						analy->free_nodes_sphere_res_factor = atoi( tokens[i + token_index+1] );
						if (analy->free_nodes_sphere_res_factor<2)
							analy->free_nodes_sphere_res_factor = 2;
						token_index+=2;
					}

					if (!strcmp( tokens[i+token_index], "scale"))
					{
						analy->free_nodes_scale_factor = atof( tokens[i + token_index+1] );
						token_index+=2;
					}

					if (!strcmp( tokens[i+token_index], "mass_scale") ||
							!strcmp( tokens[i+token_index], "ms") )
					{
						if (!strcmp( tokens[i+token_index+1], "on"))
						{
							analy->free_nodes_mass_scaling = 1;
							analy->free_nodes_vol_scaling  = 0;
						}
						if (!strcmp( tokens[i+token_index+1], "off"))
							analy->free_nodes_mass_scaling = 0;
						token_index+=2;
					}

					if (!strcmp( tokens[i+token_index], "vol_scale") ||
							!strcmp( tokens[i+token_index], "vs"))
					{
						if (!strcmp( tokens[i+token_index+1], "on"))
						{
							analy->free_nodes_vol_scaling  = 1;
							analy->free_nodes_mass_scaling = 0;
						}
						if (!strcmp( tokens[i+token_index+1], "off"))
						{
							analy->free_nodes_vol_scaling  = 0;
						}
						token_index+=2;
					}

					if (!strcmp( tokens[i+1], "mat"))
					{
						for ( i = token_index;
								i < token_cnt;
								i++ )
						{
							if ( is_numeric_range_token( tokens[i] ) )
							{
								parse_mtl_range( tokens[i], qty, &mat_min, &mat_max ) ;
								mtl = mat_min ;
								for (mtl =  mat_min ;
										mtl <= mat_max ;
										mtl++)
								{
									if (mtl > 0 && mtl <= qty)
									{
										analy->free_nodes_mats[mtl-1] = 1;
									}
								}
							}
							else
							{
								sscanf( tokens[i], "%d", &ival );
								if ( ival > 0 && ival <= qty )
									analy->free_nodes_mats[ival-1] = 1;
							}
						}
					}

					i = token_cnt;
				}
				else if (!strcmp( tokens[i], "pnhidebg") || !strcmp( tokens[i], "parthidebg") )
				{
					analy->particle_nodes_hide_background = setval;
				}
				else if (!strcmp( tokens[i], "dialog"))
				{
					env.show_dialog = setval;
				}
				/* Last On/Off switch test */
				else if (!strcmp( tokens[i], "showde"))
				{
					analy->show_deleted_elements = setval;
					analy->show_only_deleted_elements = 0;
					change_state( analy );
				}
				else if (!strcmp( tokens[i], "showonlyde") || !strcmp( tokens[i], "showode"))
				{
					analy->show_only_deleted_elements = setval;
					analy->show_deleted_elements = 0;
					change_state( analy );
				}
				else if (!strcmp( tokens[i], "showsphghost") || !strcmp( tokens[i], "showsphg"))
				{
					analy->show_sph_ghost = setval;
					change_state( analy );
				}
				else if (!strcmp( tokens[i], "master"))
				{
					analy->show_master = setval;
				}
				else if (!strcmp( tokens[i], "slave"))
				{
					analy->show_slave = setval;
				}
				else if (!strcmp( tokens[i], "selectonmat") || !strcmp( tokens[i], "som") )
				{
					analy->selectonmat = setval;
				}
				else if (!strcmp( tokens[i], "thsinglecol") || !strcmp( tokens[i], "thsc") )
				{
					analy->th_single_col = setval;
				}
				else if (!strcmp( tokens[i], "thappend") || !strcmp( tokens[i], "tha") )
				{
					analy->th_append_output = setval;
				}
				else if (!strncmp( tokens[i], "volav", 5) || !strcmp( tokens[i], "volavg") )
				{
					analy->vol_averaging = setval;
					change_state( analy );
				}
				else if(!strcmp(tokens[i], "autoselect"))
				{
					char command[2048];
					int j;
					analy->autoselect = setval;

					if(setval == TRUE)
					{
						parse_command("sw mstat", analy);
					}

					if(analy->cur_result != NULL)
					{
						load_result( analy, TRUE, TRUE, FALSE);
						if(setval == FALSE)
						{
							if(analy->selected_objects == NULL)
							{
								return;
							}

							strcpy(command, "deselect ");
							for(j = 0; j < MESH(analy).qty_class_selections; j++)
							{
								if(MESH(analy).by_class_select[j].p_class->superclass == analy->elem_state_mm.sclass[0])
								{
									break;
								}
							}
							if(j < MESH(analy).qty_class_selections)
							{
								if(analy->elem_state_mm.sclass[0] == analy->elem_state_mm.sclass[1] || analy->elem_state_mm.sclass[1] == 0)
								{
									strcat(command, MESH(analy).by_class_select[j].p_class->short_name);
									sprintf(command, "%s %d %d", command, analy->elem_state_mm.object_id[0], analy->elem_state_mm.object_id[1]);
									parse_command(command, analy);
								} 
								else
								{
									strcat(command, MESH(analy).by_class_select[j].p_class->short_name);
									sprintf(command, "%s %d", command, analy->elem_state_mm.object_id[0]);
									parse_command(command, analy);
									/* now go get the other superclass */
									for(j = 0; j < MESH(analy).qty_class_selections; j++)
									{
										if(MESH(analy).by_class_select[j].p_class->superclass == analy->elem_state_mm.sclass[1])
										{
											break;
										}
									}
									if(j < MESH(analy).qty_class_selections)
									{
										strcpy(command, "deselect ");
										strcat(command, MESH(analy).by_class_select[j].p_class->short_name);
										sprintf(command, "%s %d", command, analy->elem_state_mm.object_id[1]);
										parse_command(command, analy);
									}

								}
							}

						}
					}
				}
				else
				{
					popup_dialog( INFO_POPUP, "On/Off command unrecognized: %s\n", tokens[i] );
				}
			}
			/* Reload the result vector. */
			if ( analy->result_mod )
			{
				if ( analy->render_mode == RENDER_MESH_VIEW )
					load_result( analy, TRUE, TRUE, FALSE );
				analy->display_mode_refresh( analy );
			}
			redraw = redraw_for_render_mode( FALSE, RENDER_ANY, analy );
		}
		/*************************
		 * End of test for On/Off
		 *************************/

		else if ( strcmp( tokens[0], "switch" ) == 0 ||
				  strcmp( tokens[0], "sw" ) == 0 )
		{
			Bool_type redraw_mesh=TRUE;
			analy->draw_wireframe = FALSE;
			for ( i = 1; i < token_cnt; i++ )
			{
				/* View type. */
				if ( strcmp( tokens[i], "persp" ) == 0 )
				{
					set_orthographic( FALSE );
					set_mesh_view();
				}
				else if ( strcmp( tokens[i], "ortho" ) == 0 )
				{
					set_orthographic( TRUE );
					set_mesh_view();
				}

				/* Normals. */
				else if ( strcmp( tokens[i], "flat" ) == 0 )
				{
					analy->normals_smooth = FALSE;
					renorm = TRUE;
				}
				else if ( strcmp( tokens[i], "smooth" ) == 0 )
				{
					analy->normals_smooth = TRUE;
					renorm = TRUE;
				}

				/* Rendering style. */
				else if ( strcmp( tokens[i], "wf" ) == 0 ||
						  strcmp( tokens[i], "wireframe" ) == 0)
				{
					analy->draw_wireframe = TRUE;
					analy->mesh_view_mode = RENDER_WIREFRAME;
					update_util_panel( VIEW_WIREFRAME, NULL );
				}
				else if ( strcmp( tokens[i], "wft" ) == 0 ||
						  strcmp( tokens[i], "wireframet" ) == 0)
				{
					analy->mesh_view_mode = RENDER_WIREFRAMETRANS;
					analy->draw_wireframe_trans = TRUE;
					update_util_panel( VIEW_WIREFRAMETRANS, NULL );
				}
				else if ( strcmp( tokens[i], "hidden" ) == 0 )
				{
					analy->mesh_view_mode = RENDER_HIDDEN;
					update_util_panel( VIEW_SOLID_MESH, NULL );
				}
				else if ( strcmp( tokens[i], "solid" ) == 0 )
				{
					analy->mesh_view_mode = RENDER_FILLED;
					update_util_panel( VIEW_SOLID, NULL );
				}
				else if ( strcmp( tokens[i], "cloud" ) == 0 )
				{
					analy->mesh_view_mode = RENDER_POINT_CLOUD;
					update_util_panel( VIEW_POINT_CLOUD, NULL );
				}
				else if ( strcmp( tokens[i], "none" ) == 0 )
				{
					analy->mesh_view_mode = RENDER_NONE;
					update_util_panel( VIEW_NONE, NULL );
				}

				/* Interpolation mode. */
				else if ( strcmp( tokens[i], "noterp" ) == 0 )
					analy->interp_mode = NO_INTERP;
				else if ( strcmp( tokens[i], "interp" ) == 0 )
					analy->interp_mode = REG_INTERP;
				else if ( strcmp( tokens[i], "gterp" ) == 0 )
					analy->interp_mode = GOOD_INTERP;

				/* Mouse picking mode. */
				else if ( strcmp( tokens[i], "picsel" ) == 0 )
				{
					analy->mouse_mode = MOUSE_SELECT;
					update_util_panel( PICK_MODE_SELECT, NULL );
					redraw_mesh = FALSE;
					redraw = NO_VISUAL_CHANGE;
				}
				else if ( strcmp( tokens[i], "pichil" ) == 0 )
				{
					analy->mouse_mode = MOUSE_HILITE;
					update_util_panel( PICK_MODE_HILITE, NULL );
					redraw_mesh = FALSE;
					redraw = NO_VISUAL_CHANGE;
				}

				/* Result min/max. */
				else if( strcmp( tokens[i], "mstat" ) == 0 )
				{
					analy->use_global_mm = FALSE;
					analy->use_cumulative_mm = FALSE;
					analy->found_global_mm = FALSE;
					if ( !analy->mm_result_set[0] )
						analy->result_mm[0] = analy->state_mm[0];
					if ( !analy->mm_result_set[1] )
						analy->result_mm[1] = analy->state_mm[1];
					analy->display_mode_refresh( analy );
				}
				else if( strcmp( tokens[i], "mglob" ) == 0 )
				{
					parse_command("off autoselect", analy);
					analy->use_global_mm = TRUE;
					analy->use_cumulative_mm = FALSE;
					analy->found_global_mm = FALSE;
					get_global_minmax( analy );
					if ( !analy->mm_result_set[0] )
						analy->result_mm[0] = analy->global_mm[0];
					if ( !analy->mm_result_set[1] )
						analy->result_mm[1] = analy->global_mm[1];
					analy->display_mode_refresh( analy );
				}
				else if( strcmp( tokens[i], "mcstat" ) == 0 )
				{
					parse_command("off autoselect", analy);
					analy->use_global_mm = FALSE;
					analy->use_cumulative_mm = TRUE;
					analy->found_global_mm = FALSE;
					if ( !analy->mm_result_set[0] )
						analy->result_mm[0] = analy->global_mm[0];
					if ( !analy->mm_result_set[1] )
						analy->result_mm[1] = analy->global_mm[1];
					analy->display_mode_refresh( analy );
				}

				/* Symmetry plane behavior. */
				else if ( strcmp( tokens[i], "symcu" ) == 0 )
					analy->refl_orig_only = FALSE;
				else if ( strcmp( tokens[i], "symor" ) == 0 )
					analy->refl_orig_only = TRUE;

				/* Screen aspect ratio correction. */
				else if ( strcmp( tokens[i], "vidasp" ) == 0 )
				{
					aspect_correct( TRUE );
				}
				else if ( strcmp( tokens[i], "norasp" ) == 0 )
				{
					aspect_correct( FALSE );
				}

				/* Shell reference surface. */
				else if ( strcmp( tokens[i], "middle" ) == 0 )
				{
					old = analy->ref_surf;
					analy->ref_surf = MIDDLE;
					analy->result_mod = analy->check_mod_required( analy,
										REFERENCE_SURFACE, old,
										analy->ref_surf );
				}
				else if ( strcmp( tokens[i], "inner" ) == 0 )
				{
					old = analy->ref_surf;
					analy->ref_surf = INNER;
					analy->result_mod = analy->check_mod_required( analy,
										REFERENCE_SURFACE, old,
										analy->ref_surf );
				}
				else if ( strcmp( tokens[i], "outer" ) == 0 )
				{
					old = analy->ref_surf;
					analy->ref_surf = OUTER;
					analy->result_mod = analy->check_mod_required( analy,
										REFERENCE_SURFACE, old,
										analy->ref_surf );
				}

				/* Strain basis. */
				else if ( strcmp( tokens[i], "rglob" ) == 0 )
				{
					old = analy->ref_frame;
					analy->ref_frame = GLOBAL;
					analy->result_mod = analy->check_mod_required( analy,
										REFERENCE_FRAME, old,
										analy->ref_frame );
				}
				else if ( strcmp( tokens[i], "rloc" ) == 0 )
				{
					old               = analy->ref_frame;
					analy->ref_frame  = LOCAL;
					analy->result_mod = analy->check_mod_required( analy,
										REFERENCE_FRAME, old,
										analy->ref_frame );
					if ( old != LOCAL )
						/* LOCAL and analy->do_tensor_transform are exclusive */
						analy->do_tensor_transform = FALSE;

					change_state( analy );
					redraw = BINDING_MESH_VISUAL;
				}

				/* Strain variety. */
				else if ( strcmp( tokens[i], "infin" ) == 0 )
				{
					if (analy->cur_result != NULL && is_primal_quad_strain_result( analy->cur_result->name ))
					{
						popup_dialog( USAGE_POPUP,
									  "Cannot set strain variety for a primal shell strain" );
						valid_command = FALSE;
					}
					old = analy->strain_variety;
					analy->strain_variety = INFINITESIMAL;
					analy->result_mod = analy->check_mod_required( analy,
										STRAIN_TYPE, old,
										analy->strain_variety );
				}
				else if ( strcmp( tokens[i], "grn" ) == 0 )
				{
					old = analy->strain_variety;
					analy->strain_variety = GREEN_LAGRANGE;
					analy->result_mod = analy->check_mod_required( analy,
										STRAIN_TYPE, old,
										analy->strain_variety );
				}
				else if ( strcmp( tokens[i], "alman" ) == 0 )
				{
					old = analy->strain_variety;
					analy->strain_variety = ALMANSI;
					analy->result_mod = analy->check_mod_required( analy,
										STRAIN_TYPE, old,
										analy->strain_variety );
				}
				else if ( strcmp( tokens[i], "rate" ) == 0 )
				{
					old = analy->strain_variety;
					analy->strain_variety = RATE;
					analy->result_mod = analy->check_mod_required( analy,
										STRAIN_TYPE, old,
										analy->strain_variety );
				}
				else if ( strcmp( tokens[i], "grdvec" ) == 0 )
				{
					analy->vectors_at_nodes = FALSE;
				}
				else if ( strcmp( tokens[i], "nodvec" ) == 0 )
				{
					analy->vectors_at_nodes = TRUE;
				}
				else if ( strcmp( tokens[i], "glyphalign" ) == 0 )
				{
					set_glyph_alignment( GLYPH_ALIGN );
				}
				else if ( strcmp( tokens[i], "glyphstag" ) == 0 )
				{
					set_glyph_alignment( GLYPH_STAGGERED );
				}
				else if ( strcmp( tokens[i], "derived_results" ) == 0 ){
					analy->result_source = DERIVED;
					analy->result_mod = refresh_shown_result( analy );
				}
				else if ( strcmp( tokens[i], "primal_results" ) == 0 ){
					analy->result_source = PRIMAL;
					analy->result_mod = refresh_shown_result( analy );
				}
				else if ( strcmp( tokens[i], "all_results" ) == 0 ){
					analy->result_source = ALL;
					analy->result_mod = refresh_shown_result( analy );
				}
				else if ( strcmp( tokens[i], "derive_from_primals" ) == 0 )
				{
					analy->preferred_primal_source = PRIMAL;
					analy->result_mod = refresh_shown_result( analy );
				}
				else if ( strcmp( tokens[i], "derive_from_derived" ) == 0 )
				{
					analy->preferred_primal_source = DERIVED;
					analy->result_mod = refresh_shown_result( analy );
				}
				else
					popup_dialog( INFO_POPUP, "Switch command unrecognized: %s\n", tokens[i] );
			}

			/* Reload the result vector. */
			if ( analy->result_mod )
			{
				if ( analy->render_mode == RENDER_MESH_VIEW )
					load_result( analy, TRUE, TRUE, FALSE );
				analy->display_mode_refresh( analy );
			}
			if ( redraw_mesh )
				redraw = redraw_for_render_mode( FALSE, RENDER_ANY, analy );
		} /* end sw */
		else if ( strcmp( tokens[0], "setpath" ) == 0 )
		{
			if ( token_cnt == 1 ) /* Clear path 0 */
			{
				if ( analy->paths[0] && analy->paths_set[0] )
				{
					free( analy->paths[0] );
					analy->paths_set[0] = FALSE;
				}
			}

			if ( token_cnt == 2 ) /* Clear the specified path */
			{
				int path_id=1;
				path_id = atoi( tokens[i+1] );
				if ( path_id>0 && path_id<=MAX_PATHS )
				{
					if ( analy->paths[path_id-1] && analy->paths_set[path_id-1] )
					{
						free( analy->paths[path_id-1] );
						analy->paths_set[path_id-1] = FALSE;
					}
				}
				else
				{
					analy->paths[0] = strdup( tokens[1] );
					if ( analy->paths[0][strlen(analy->paths[0] )-1] == '/' )
						analy->paths[0][strlen(analy->paths[0] )-1] = '\0';
					analy->paths_set[0] = TRUE;
				}
			}

			if ( token_cnt == 3 ) /* Set the specified path */
			{
				int path_id=1;
				path_id = atoi( tokens[1] );
				if ( path_id>0 && path_id<=MAX_PATHS )
					if ( analy->paths[path_id-1] && analy->paths_set[path_id-1] )
					{
						free( analy->paths[path_id-1] );
					}
				analy->paths[path_id-1] = strdup( tokens[2] );
				if ( analy->paths[path_id-1][strlen(analy->paths[path_id-1] )-1] == '/' )
					analy->paths[path_id-1][strlen(analy->paths[path_id-1] )-1] = '\0';
				analy->paths_set[path_id-1] = TRUE;
			}
		}
		else if ( strcmp( tokens[0], "clrpath" ) == 0 )
		{
			if ( token_cnt == 2 )
			{
				int path_id=1;
				path_id = atoi( tokens[1] );
				string_to_upper( tokens[1], tmp_token ); /* Make case insensitive */
				if ( !strcmp( "ALL", tmp_token) )   /* Clear all path entries */
				{
					for ( path_index=0;
							path_index<MAX_PATHS;
							path_index++ )
						if ( analy->paths[path_index] && analy->paths_set[path_index] )
						{
							free( analy->paths[path_index] );
							analy->paths_set[path_index] = FALSE;
						}
				}
				else
				{
					if ( path_id>0 && path_id<=MAX_PATHS ) /* Clear the specified path */
						if ( analy->paths[path_id-1] && analy->paths_set[path_id-1] )
						{
							free( analy->paths[path_id-1] );
							analy->paths_set[path_id-1] = FALSE;
						}
				}
			}
			else
			{
				if ( analy->paths[0] && analy->paths_set[0] )
				{
					free( analy->paths[0] );
					analy->paths_set[0] = FALSE;
				}
			}
		}
		else if ( strcmp( tokens[0], "showpath" ) == 0 )
		{
			printf("\nPaths Set:\n");
			for ( path_index=0;
					path_index<MAX_PATHS;
					path_index++ )
				if ( analy->paths[path_index] && analy->paths_set[path_index] )
					printf("\n(%d)\t\t%s", path_index+1, analy->paths[path_index] );
				else
					printf("\n(%d)\t\tNot Defined", path_index+1 );
			printf("\n" );
		}
		else if ( strcmp( tokens[0], "setpick" ) == 0 )
		{
			if ( token_cnt == 3 )
			{
				ival = atoi( tokens[1] );
				if ( ival == 1 )
					btn_type = BTN1_PICK;
				else if ( ival == 2 )
					btn_type = BTN2_PICK;
				else if ( ival == 3 )
					btn_type = BTN3_PICK;
				else
					valid_command = FALSE;

				if ( valid_command )
				{
					p_mo_class = find_named_class( MESH_P( analy ), tokens[2] );
					if ( p_mo_class != NULL )
						set_pick_class( p_mo_class, btn_type );
					else
						valid_command = FALSE;
				}
			}
			else
				valid_command = FALSE;

			if ( valid_command )
				update_util_panel( btn_type, p_mo_class );
			else
				popup_dialog( USAGE_POPUP,
							  "setpick <button (ie,  1, 2, or 3)> <class name>" );
		}
		else if ( strcmp( tokens[0], "fracsz" ) == 0 ||
				  strcmp( tokens[0], "fracsize" ) == 0 )
		{
			sscanf( tokens[1], "%d", &ival );
			if ( ival >= 0 && ival <= 13 )
			{
				analy->float_frac_size = ival;
				redraw = redraw_for_render_mode( FALSE, RENDER_ANY, analy );
			}
		}
		else if ( strcmp( tokens[0], "fractsz" ) == 0 ||
				  strcmp( tokens[0], "fractimesize" ) == 0 )
		{
			sscanf( tokens[1], "%d", &ival );
			if ( ival >= 0 && ival <= 13 )
			{
				analy->time_frac_size = ival;
				redraw = redraw_for_render_mode( FALSE, RENDER_ANY, analy );
			}
		}
		else if ( strcmp( tokens[0], "hidwid" ) == 0 )
		{
			sscanf( tokens[1], "%f", &val );
			if ( token_cnt == 2 && val > 0.0 )
			{
				analy->hidden_line_width = val;
				redraw = redraw_for_render_mode( FALSE, RENDER_MESH_VIEW, analy );
			}
			else
				popup_dialog( USAGE_POPUP, "hidwid <line width>" );
		}
		else if ( strcmp( tokens[0], "dumpresult" ) == 0 ||
				  strcmp( tokens[0], "dumpres" ) == 0 )
		{
			if ( token_cnt == 1 )
				dump_result( analy, NULL ) ;
			else if ( token_cnt == 2 )   /* First argument is the result to show */
			{
				strcpy( tmp_token, "show " );
				strcat( tmp_token, tokens[1] );
				parse_command( tmp_token, analy );
				dump_result( analy, NULL ) ;
			}
			else if ( token_cnt == 3 )   /* 3rd argumnet is the filename */
			{
				strcpy( tmp_token, "show " );
				strcat( tmp_token, tokens[1] );
				parse_command( tmp_token, analy );
				dump_result( analy, tokens[2] );
			}
		}


		/****************************************
		 * VIS/INVIS
		 *****************************************/


		else if ( strcmp( tokens[0], "invis" ) == 0 ||
				  strcmp( tokens[0], "vis" ) == 0 	||
				  strcmp( tokens[0], "enable" ) == 0 	||
				  strcmp( tokens[0], "disable" ) == 0 	)
		{
			//process which commands are included
			Bool_type vis = False;
			Bool_type invis = False;
			Bool_type enable = False;
			Bool_type disable = False;
			int post_cmd_pos = 0;
			idx = 0;
			while(	(post_cmd_pos < token_cnt) 							&&
				    (strcmp( tokens[post_cmd_pos], "invis" ) == 0 		||
					 strcmp( tokens[post_cmd_pos], "vis" ) == 0 		||
					 strcmp( tokens[post_cmd_pos], "enable" ) == 0 		||
					 strcmp( tokens[post_cmd_pos], "disable" ) == 0)	)
			{
				if(	strcmp( tokens[post_cmd_pos], "invis" ) == 0	){
					invis = True;
				}
				if(	strcmp( tokens[post_cmd_pos], "vis" ) == 0	){
					vis = True;
				}
				if(	strcmp( tokens[post_cmd_pos], "enable" ) == 0	){
					enable = True;
				}
				if(	strcmp( tokens[post_cmd_pos], "disable" ) == 0	){
					disable = True;
				}
				post_cmd_pos++;
			}

			if(vis && invis){
				//error
			}
			if(enable && disable){
				//error
			}


			//vis/invis section
			if(vis || invis){
				//set to the number of additional commands
				idx = post_cmd_pos-1;

				if (invis)
				{
					setval       = TRUE;
					vis_selected = FALSE;
				}
				else
				{
					setval       = FALSE;
					vis_selected = TRUE;
				}


				mat_selected    = TRUE;
				result_selected = FALSE;
				p_class         = NULL;

				qty        = MESH( analy ).material_qty;
				p_uc       = MESH( analy ).hide_material;
				mat_qty    = MESH( analy ).material_qty;
				p_hide_qty = &MESH( analy ).mat_hide_qty;
				p_elem     = NULL;

				// Look for element selection keyword
				string_to_upper( tokens[post_cmd_pos], tmp_token ); // Make case insensitive

				if( strcmp( tmp_token, "ELEM" ) == 0 )
				{
					mat_selected = FALSE;
					string_to_upper( tokens[2+idx], tmp_token ); // Make case insensitive
					idx++;
				}

				if( strcmp( tmp_token, "RESULT" ) == 0 )
				{
					result_selected = TRUE;
					mat_selected    = FALSE;
					string_to_upper( tokens[2+idx], tmp_token ); // Make case insensitive
					idx++;
				}

				if( strcmp( tokens[post_cmd_pos], "SURF" ) == 0 )
				{
					qty  = MESH( analy ).surface_qty;
					p_uc = MESH( analy ).hide_surface;
					idx++;
				}

				if( strcmp( tmp_token, "ELEM" ) == 0 )
				{
					mat_selected = FALSE;
					string_to_upper( tokens[2+idx], tmp_token ); // Make case insensitive
					strcpy( tmp_token, tokens[1+idx] );
					idx++;
				}

				class_selected = FALSE;

				for(i = 0; i < MESH(analy).qty_class_selections; i++)
				{
					p_class = MESH(analy).by_class_select[i].p_class;
					if(strcmp(tokens[1+idx] , p_class->short_name) == 0)
					{
						class_selected = TRUE;
						break;
					}
				}
				if(class_selected == FALSE)
				{
					p_class = NULL;
				}

				if( class_selected == TRUE  && is_elem_class(p_class->superclass))
				{

					elem_qty = MESH( analy ).by_class_select[i].p_class->qty;
					p_hide_elem_qty =  &MESH( analy ).by_class_select[i].hide_class_elem_qty;
					p_elem          = MESH( analy ).by_class_select[i].hide_class_elem;
					mat_selected = FALSE;
					class_selected = TRUE;
					idx++;
				}

				if ( !strcmp( "BRICK", tmp_token ) && !class_selected )
				{
					string_to_upper( tokens[2+idx], tmp_token );      // Make case insensitive
					mat_selected = FALSE;
					p_uc = MESH( analy ).hide_brick;

					if ( result_selected )
					{
						MESH( analy ).hide_brick_by_result = TRUE;
						MESH( analy ).brick_result_min = atof( tokens[3+idx] );
						MESH( analy ).brick_result_max = atof( tokens[4+idx] );
						idx+=3;
					}

					if (!mat_selected && !result_selected)
					{
						MESH( analy ).hide_brick_by_mat    = FALSE;
						MESH( analy ).hide_brick_by_result = FALSE;

						// Hide bricks by elements
						elem_qty        =  MESH( analy ).brick_qty;
						p_elem          =  MESH( analy ).hide_brick_elem;
						p_hide_elem_qty =  &MESH( analy ).hide_brick_elem_qty;
					}

					idx++;
				}

				if ( !strcmp( "SHELL", tmp_token ) && !class_selected )
				{
					string_to_upper( tokens[2+idx], tmp_token );      // Make case insensitive
					mat_selected = FALSE;
					p_uc = MESH( analy ).hide_shell;

					if ( result_selected )
					{
						MESH( analy ).hide_shell_by_result = TRUE;
						MESH( analy ).shell_result_min = atof( tokens[3+idx] );
						MESH( analy ).shell_result_max = atof( tokens[4+idx] );
						idx+=3;
					}

					if (!mat_selected && !result_selected)
					{
						MESH( analy ).hide_shell_by_mat    = FALSE;
						MESH( analy ).hide_shell_by_result = FALSE;

						// Hide shells by elements
						elem_qty        =  MESH( analy ).shell_qty;
						p_elem          =  MESH( analy ).hide_shell_elem;
						p_hide_elem_qty =  &MESH( analy ).hide_shell_elem_qty;
					}

					idx++;
				}

				if ( !strcmp( "TRUSS", tmp_token ) && !class_selected )
				{
					string_to_upper( tokens[2+idx], tmp_token );      // Make case insensitive
					mat_selected = FALSE;
					p_uc = MESH( analy ).hide_truss;

					if ( result_selected )
					{
						MESH( analy ).hide_truss_by_result = TRUE;
						MESH( analy ).truss_result_min = atof( tokens[3+idx] );
						MESH( analy ).truss_result_max = atof( tokens[4+idx] );
						idx+=3;
					}

					if (!mat_selected && !result_selected)
					{
						MESH( analy ).hide_truss_by_mat    = FALSE;
						MESH( analy ).hide_truss_by_result = FALSE;

						// Hide trusss by elements
						elem_qty        =  MESH( analy ).truss_qty;
						p_elem          =  MESH( analy ).hide_truss_elem;
						p_hide_elem_qty =  &MESH( analy ).hide_truss_elem_qty;
					}

					idx++;
				}

				if ( !strcmp( "BEAM", tmp_token ) && !class_selected )
				{
					string_to_upper( tokens[2+idx], tmp_token );      // Make case insensitive
					mat_selected = FALSE;
					p_uc = MESH( analy ).hide_beam;

					if ( result_selected )
					{
						MESH( analy ).hide_beam_by_result = TRUE;
						MESH( analy ).beam_result_min = atof( tokens[3+idx] );
						MESH( analy ).beam_result_max = atof( tokens[4+idx] );
						idx+=3;
					}

					if (!mat_selected && !result_selected)
					{
						MESH( analy ).hide_beam_by_mat    = FALSE;
						MESH( analy ).hide_beam_by_result = FALSE;

						// Hide beams by elements
						elem_qty        =  MESH( analy ).beam_qty;
						p_elem          =  MESH( analy ).hide_beam_elem;
						p_hide_elem_qty =  &MESH( analy ).hide_beam_elem_qty;
					}

					idx++;
				}

				// Particle type objects
				if ( !strcmp( tmp_token, "PART" ) || is_particle_class( analy, -1, tmp_token ) && !class_selected )
				{
					if ( result_selected )
					{
						MESH( analy ).hide_particle_by_result = TRUE;
						MESH( analy ).particle_result_min = atof( tokens[3+idx] );
						MESH( analy ).particle_result_max = atof( tokens[4+idx] );
						idx+=3;
					}
					else
						MESH( analy ).hide_particle_by_result = FALSE;

					mat_selected = FALSE;
					p_uc        = MESH( analy ).hide_particle;
					p_hide_qty  = &MESH( analy ).particle_hide_qty;
					p_elem      = NULL;

					if (!mat_selected && !result_selected)
					{
						MESH( analy ).hide_particle_by_mat    = FALSE;
						MESH( analy ).hide_particle_by_result = FALSE;

						// Hide particles by elements
						elem_qty        =  MESH( analy ).particle_qty;
						p_elem          =  MESH( analy ).hide_particle_elem;
						p_hide_elem_qty =  &MESH( analy ).hide_particle_elem_qty;
					}
					idx++;
				}

				if ( !result_selected )
				{
					process_mat_obj_selection ( analy,  tokens, idx, token_cnt, mat_qty,
												elem_qty, p_class,
												p_elem, p_hide_qty, p_hide_elem_qty,
												p_uc,
												setval, vis_selected, mat_selected );

					// handle the case where we invised by a material number(s) but vised by element class
					//  *             this requires vising by material number(s)
					if(class_selected == TRUE && strcmp(tokens[0], "vis") == 0)
					{
						for(i = 1; i < token_cnt; i++)
						{
							if(isdigit(tokens[i][0]))
							{
								break;
							}
						}
						strcpy(tmp_tokens[0], tokens[0]);
						j = 1;
						for(; i < token_cnt; i++)
						{
							strcpy(tmp_tokens[j], tokens[i]);
							j++;
						}

						mat_qty = MESH( analy ).material_qty;
						p_hide_qty = &MESH( analy ). mat_hide_qty;
						p_uc = MESH( analy ).hide_material;
						process_mat_obj_selection ( analy,  tokens, 0, token_cnt, mat_qty,
													elem_qty, p_class,
													p_elem, p_hide_qty, p_hide_elem_qty,
													p_uc,
													setval, vis_selected, TRUE );

					}

					if(class_selected == FALSE)
					{
						for(i = 0; i < MESH(analy).qty_class_selections; i++)
						{
							p_class = MESH(analy).by_class_select[i].p_class;
							elem_qty = p_class->qty;
							p_hide_elem_qty = &MESH(analy).by_class_select[i].hide_class_elem_qty;
							p_elem = MESH(analy).by_class_select[i].hide_class_elem;
							process_mat_obj_selection ( analy,  tokens, idx, token_cnt, mat_qty,
														elem_qty, p_class,
														p_elem, p_hide_qty, p_hide_elem_qty,
														p_uc,
														setval, vis_selected, 0 );
						}
					}


					// now get all the element classes associated with this material, but only if token_cnt == 2
					strcpy(comment, tokens[1]);
					comment[1] = '\0';
					i = atoi(comment);
					if(token_cnt == 2  || ((token_cnt > 2 && i > 0) || !strcmp(tokens[1], "allb")))
					{

						if(!strcmp(tokens[1], "allb"))
						{
							incr = 1;
							new_token_cnt = 1;
							strcpy(newtokens[0], tokens[0]);
							p_uc2 = MESH(analy).hide_material;
							// p_uc2 is of type unsigned char *

							for(i = 0; i < mat_qty; i++)
							{
								if(p_uc2[i])
								{
									sprintf(newtokens[incr],"%d", i + 1);
									incr++;
									new_token_cnt++;
								}
							}

						}
						idx = 0;
						mat_selected = FALSE;
						class_selected = TRUE;

						for(i = 0; i < MESH(analy).qty_class_selections; i++)
						{
							p_class = MESH(analy).by_class_select[i].p_class;


							if( (i < MESH(analy).qty_class_selections) && is_elem_class(p_class->superclass))
							{

								elem_qty = MESH( analy ).by_class_select[i].p_class->qty;
								p_hide_elem_qty =  &MESH( analy ).by_class_select[i].hide_class_elem_qty;
								p_elem          = MESH( analy ).by_class_select[i].hide_class_elem;

								if(!strcmp(tokens[1], "allb"))
								{
									process_mat_obj_selection ( analy,  newtokens, idx, new_token_cnt, mat_qty,
																elem_qty, p_class,
																p_elem, p_hide_qty, p_hide_elem_qty,
																p_uc,
																setval, vis_selected, mat_selected );
								}
								else
								{
									process_mat_obj_selection ( analy,  tokens, idx, token_cnt, mat_qty,
																elem_qty, p_class,
																p_elem, p_hide_qty, p_hide_elem_qty,
																p_uc,
																setval, vis_selected, mat_selected );
								}


							}
						}
					}


				}

			}

			//enable/disable
			if(enable || disable){
				//set to the number of additional commands
				idx = post_cmd_pos-1;
				if (disable)
				{
					setval = TRUE;
					enable_selected = FALSE;
				}
				else
				{
					setval = FALSE;
					enable_selected = TRUE;
				}

				mat_selected    = TRUE;
				result_selected = FALSE;
				p_class         = NULL;

				qty        = MESH( analy ).material_qty;
				p_uc       = MESH( analy ).disable_material;
				mat_qty    = MESH( analy ).material_qty;
				p_hide_qty = &MESH( analy ).mat_disable_qty;
				p_elem     = NULL;

				// Look for element selection keyword
				string_to_upper( tokens[1+idx], tmp_token ); // Make case insensitive

				if( strcmp( tmp_token, "ELEM" ) == 0 )
				{
					mat_selected = FALSE;
					string_to_upper( tokens[2+idx], tmp_token ); // Make case insensitive
					idx++;
				}

				// Result range selection
				if( strcmp( tmp_token, "RESULT" ) == 0 )
				{
					mat_selected    = FALSE;
					result_selected = TRUE;
					string_to_upper( tokens[2+idx], tmp_token ); // Make case insensitive
					idx++;
				}

				if( strcmp( tokens[1], "SURF" ) == 0 )
				{
					mat_qty = MESH( analy ).surface_qty;
					p_uc    = MESH( analy ).disable_surface;
					idx++;
				}

				class_selected = FALSE;

				for(i = 0; i < MESH(analy).qty_class_selections; i++)
				{
					p_class = MESH(analy).by_class_select[i].p_class;
					if(strcmp(tokens[1+idx] , p_class->short_name) == 0)
					{
						class_selected = TRUE;
						break;
					}
				}
				if(class_selected == FALSE)
				{
					p_class = NULL;
				}

				if( class_selected == TRUE  && is_elem_class(p_class->superclass))
				{

					elem_qty = MESH( analy ).by_class_select[i].p_class->qty;
					p_hide_elem_qty =  &MESH( analy ).by_class_select[i].hide_class_elem_qty;
					p_disable_elem_qty = &MESH( analy ).by_class_select[i].disable_class_elem_qty;
					p_elem          = MESH( analy ).by_class_select[i].disable_class_elem;
					mat_selected = FALSE;
					class_selected = TRUE;
					idx++;
				}
				else if(token_cnt > 2 && strcmp(tokens[0], "include") == 0 && strcmp(tokens[1], "all") == 0)
				{
					return;
				}


				if ( !strcmp( "BRICK", tmp_token ) && !class_selected )
				{
					string_to_upper( tokens[2+idx], tmp_token );      // Make case insensitive
					mat_selected = FALSE;
					p_uc =  MESH( analy ).disable_brick;
					mat_selected = FALSE;

					if ( result_selected )
					{
						MESH( analy ).brick_result_min = atof( tokens[3+idx] );
						MESH( analy ).brick_result_max = atof( tokens[4+idx] );
						idx+=3;
					}
					else
					{
						MESH( analy ).disable_brick_by_mat = FALSE;

						// Disable bricks by elements
						elem_qty           =  MESH( analy ).brick_qty;
						p_elem             =  MESH( analy ).disable_brick_elem;
						p_disable_elem_qty =  &MESH( analy ).disable_brick_elem_qty;
					}

					idx++;
				}

				if ( !strcmp( "SHELL", tmp_token ) && !class_selected )
				{
					string_to_upper( tokens[2+idx], tmp_token );  // Make case insensitive
					p_uc =  MESH( analy ).disable_shell;
					mat_selected = FALSE;
					if ( result_selected )
					{
						MESH( analy ).shell_result_min = atof( tokens[3+idx] );
						MESH( analy ).shell_result_max = atof( tokens[4+idx] );
						idx+=3;
					}
					else
					{
						MESH( analy ).disable_shell_by_mat = FALSE;

						// Disable shells by elements
						elem_qty           =  MESH( analy ).shell_qty;
						p_elem             =  MESH( analy ).disable_shell_elem;
						p_disable_elem_qty =  &MESH( analy ).disable_shell_elem_qty;
					}
					idx++;
				}

				if ( !strcmp( "TRUSS", tmp_token ) && !class_selected )
				{
					string_to_upper( tokens[2+idx], tmp_token );      // Make case insensitive
					p_uc =  MESH( analy ).disable_truss;
					mat_selected = FALSE;
					if ( result_selected )
					{
						MESH( analy ).truss_result_min = atof( tokens[3+idx] );
						MESH( analy ).truss_result_max = atof( tokens[4+idx] );
						idx+=3;
					}
					else
					{
						MESH( analy ).disable_truss_by_mat=FALSE;

						// Disable trusss by elements
						elem_qty           =  MESH( analy ).truss_qty;
						p_elem             =  MESH( analy ).disable_truss_elem;
						p_disable_elem_qty =  &MESH( analy ).disable_brick_elem_qty;
					}

					idx++;
				}

				if ( !strcmp( "BEAM", tmp_token ) && !class_selected )
				{
					string_to_upper( tokens[2+idx], tmp_token );      // Make case insensitive
					p_uc =  MESH( analy ).disable_beam;
					mat_selected = FALSE;
					if ( result_selected )
					{
						MESH( analy ).beam_result_min = atof( tokens[3+idx] );
						MESH( analy ).beam_result_max = atof( tokens[4+idx] );
						idx+=3;
					}
					else
					{
						MESH( analy ).disable_beam_by_mat=FALSE;

						// Disable beams by elements
						elem_qty           =  MESH( analy ).beam_qty;
						p_elem             =  MESH( analy ).disable_beam_elem;
						p_disable_elem_qty =  &MESH( analy ).disable_beam_elem_qty;
					}
					idx++;
				}

				if ( !strcmp( tmp_token, "PART" )  || is_particle_class( analy, -1, tmp_token ) && !class_selected )
				{
					if ( result_selected )
					{
						MESH( analy ).particle_result_min = atof( tokens[3+idx] );
						MESH( analy ).particle_result_max = atof( tokens[4+idx] );
						idx+=3;
					}

					p_uc       = MESH( analy ).disable_particle;
					p_hide_qty = &MESH( analy ).particle_disable_qty;

					elem_qty   = mat_qty;
					idx++;
					mat_selected = TRUE;
				}

				if ( !result_selected )
				{
					if(class_selected == FALSE)
					{
						process_mat_obj_selection ( analy,  tokens, idx, token_cnt, mat_qty,
													elem_qty, p_class,
													p_elem, p_hide_qty, p_disable_elem_qty,
													p_uc,
													setval, enable_selected, mat_selected );


					// handle the case where we disabled by a material number(s) but enabled by element class
					// *             this requires enabling by material number(s)

						for( i = 0; i < MESH(analy).qty_class_selections; i++)
						{
						  elem_qty = MESH(analy).by_class_select[i].p_class->qty;
						  p_disable_elem_qty = &MESH(analy).by_class_select[i].disable_class_elem_qty;
						  p_elem = MESH(analy).by_class_select[i].disable_class_elem;
						  p_class = MESH(analy).by_class_select[i].p_class;

						  process_mat_obj_selection ( analy,  tokens, idx, token_cnt, mat_qty,
													  elem_qty, p_class,
													  p_elem, p_disable_qty, p_disable_elem_qty,
													  p_uc,
													  setval, enable_selected, 0 );


						}

					}
					else
					{
						for(j = 1; j < token_cnt; j++)
						{
							if(isdigit(tokens[j][0]))
							{
								j--;
								break;
							}
						}

						elem_qty = MESH(analy).by_class_select[i].p_class->qty;
						p_hide_elem_qty = &MESH(analy).by_class_select[i].hide_class_elem_qty;
						p_disable_elem_qty = &MESH(analy).by_class_select[i].disable_class_elem_qty;
						p_elem = MESH(analy).by_class_select[i].disable_class_elem;

						process_mat_obj_selection ( analy,  tokens, j, token_cnt, mat_qty,
													elem_qty, p_class,
													p_elem, p_disable_qty, p_disable_elem_qty,
													p_uc,
													setval, enable_selected, 0 );


					}

					if(class_selected == TRUE && strcmp(tokens[0], "enable") == 0)
					{
						for(i = 1; i < token_cnt; i++)
						{
							if(isdigit(tokens[i][0]))
							{
								break;
							}
						}
						strcpy(tmp_tokens[0], tokens[0]);
						j = 1;
						for(; i < token_cnt; i++)
						{
							strcpy(tmp_tokens[j], tokens[i]);
							j++;
						}

						mat_qty = MESH( analy ).material_qty;
						p_hide_qty = &MESH( analy ). mat_hide_qty;
						p_uc = MESH( analy ).disable_material;
						process_mat_obj_selection ( analy,  tokens, 0, token_cnt, mat_qty,
													elem_qty, p_class,
													p_elem, p_hide_qty, p_hide_elem_qty,
													p_uc,
													setval, enable_selected, TRUE );


					}


				}

				process_node_selection ( analy );

				if ( analy->show_cut || analy->show_roughcut )
					parse_command( "on cut", analy );

				load_result( analy, TRUE, TRUE, FALSE );
				analy->result_mod = TRUE;
				redraw = BINDING_MESH_VISUAL;



			}

			if(vis || invis){
				// for element class G_PARTICLE the user can affect the visibility of these particles by typing
				//     vis pn   or
				//     invis pn
				//     Code added by Bill Oliver on 9/13/2013

				if ( !strcmp(tokens[1], "pn"))
				{
					for(i = 0; i < MESH(analy).qty_class_selections; i++)
					{
						p_class = MESH(analy).by_class_select[i].p_class;
						if(p_class->superclass == G_PARTICLE)
						{
							p_hide_elem_qty = &MESH(analy).by_class_select[i].hide_class_elem_qty;
							p_elem = MESH(analy).by_class_select[i].hide_class_elem;
							for(j = 0; j < p_class->qty; j++)
							{
								if(!vis_selected)
								{
									p_elem[j] = TRUE;
									(*p_hide_elem_qty)++;
								}
								else
								{
									p_elem[j] = FALSE;
									(*p_hide_elem_qty)--;
								}
							}
						}
					}
				}
				reset_face_visibility( analy );
				if ( analy->dimension == 3 ) renorm = TRUE;

				if ( analy->hide_edges_by_mat )
					get_mesh_edges( analy->cur_mesh_id, analy );
				if ( analy->show_cut || analy->show_roughcut )
					parse_command( "on cut", analy );

			}
			process_node_selection ( analy );
			if(enable || disable){
				if ( analy->show_cut || analy->show_roughcut )
					parse_command( "on cut", analy );
				load_result( analy, TRUE, TRUE, FALSE );
			}

			analy->result_mod = TRUE;
			redraw = BINDING_MESH_VISUAL;

		}

		/****************************************
		 * INCLUDE/EXCLUDE
		 ****************************************
		 */
		else if ( strcmp( tokens[0],  "exclude" ) == 0 ||
				  strcmp( tokens[0], "include" ) == 0 )
		{
			if ( strcmp( tokens[0],  "exclude" ) == 0 )
			{
				setval           = TRUE;
				include_selected = FALSE;
			}
			else
			{
				setval           = FALSE;
				include_selected = TRUE;
			}

			mat_selected    = TRUE;
			result_selected = FALSE;
			idx = 0;
			mat_qty    = MESH( analy ).material_qty;
			p_hide_qty = &MESH( analy ).mat_hide_qty;
			p_uc    = MESH( analy ).hide_material;
			p_uc2   = MESH( analy ).hide_material;
			p_elem  = NULL;
			p_elem2 = NULL;
			p_class = NULL;
			p_hide_elem_qty = NULL;
			p_disable_elem_qty = NULL;

			/* Look for element selection keyword */
			string_to_upper( tokens[1], tmp_token ); /* Make case insensitive */

			if( strcmp( tmp_token, "ELEM" ) == 0 )
			{
				mat_selected = FALSE;
				string_to_upper( tokens[2], tmp_token ); /* Make case insensitive */
				idx++;
			}

			/* Result range selection */
			if( strcmp( tmp_token, "RESULT" ) == 0 )
			{
				mat_selected    = FALSE;
				result_selected = TRUE;
				string_to_upper( tokens[2], tmp_token ); /* Make case insensitive */
				idx++;
			}

			if( strcmp( tmp_token, "SURF" ) == 0 )
			{
				mat_qty = MESH( analy ).surface_qty;
				p_uc    = MESH( analy ).disable_surface;
				p_uc2   = MESH( analy ).hide_surface;
				idx++;
			}

			class_selected = FALSE;
			/*if ( is_elem_class( tokens[1] ) ) {
				 p_class = NULL;
					 class_select_index = get_class_select_index( analy, tokens[1] );
				 if ( class_select_index>=0 ) {
					  p_class  =  MESH( analy ).by_class_select[class_select_index].p_class;
					  elem_qty = MESH( analy ).by_class_select[class_select_index].p_class->qty;
				  p_hide_elem_qty =  &MESH( analy ).by_class_select[class_select_index].hide_class_elem_qty;
				  p_elem          = MESH( analy ).by_class_select[class_select_index].hide_class_elem;
				  mat_selected = FALSE;
				 }

				 if ( p_class )
					  class_selected = TRUE;
				 idx++;
			}*/

			for(i = 0; i < MESH(analy).qty_class_selections; i++)
			{
				p_class = MESH(analy).by_class_select[i].p_class;
				if(strcmp(tokens[1], p_class->short_name) == 0)
				{
					class_selected = TRUE;
					break;
				}
			}
			if(class_selected == FALSE)
			{
				p_class = NULL;
			}

			if( class_selected == TRUE && is_elem_class(p_class->superclass))
			{

				elem_qty = MESH( analy ).by_class_select[i].p_class->qty;
				p_hide_elem_qty =  &MESH( analy ).by_class_select[i].hide_class_elem_qty;
				p_disable_elem_qty = &MESH( analy ).by_class_select[i].disable_class_elem_qty;
				p_elem          = MESH( analy ).by_class_select[i].exclude_class_elem;
				mat_selected = FALSE;
				idx++;
			}
			else if(token_cnt > 2 && strcmp(tokens[0], "include") == 0 && strcmp(tokens[1], "all") == 0)
			{
				return;
			}

			 /* Particle type objects */
			/* added && !class_selected to the condifional by Bill Oliver as a test */
			if ( !strcmp( tmp_token, "PART" )  || is_particle_class( analy, -1, tmp_token )  &&
					!class_selected)
			{
				if ( result_selected )
				{
					MESH( analy ).hide_particle_by_result = TRUE;
					MESH( analy ).particle_result_min = atof( tokens[3] );
					MESH( analy ).particle_result_max = atof( tokens[4] );
					idx+=3;
				}
				else MESH( analy ).hide_particle_by_result = FALSE;

				p_uc  = MESH( analy ).disable_particle;
				p_uc2 = MESH( analy ).hide_particle;

				p_hide_qty = &MESH( analy ).particle_disable_qty;
				elem_qty   = mat_qty;
				idx++;
				mat_selected = TRUE;
			}

			if ( !result_selected )
			{
			   if( class_selected == FALSE )
			   {
				   process_mat_obj_selection ( analy,  tokens, idx, token_cnt, mat_qty,
											   elem_qty, p_class,
											   p_elem, p_hide_qty, p_hide_elem_qty,
											   p_uc,
											   setval, include_selected, mat_selected );

				   p_disable_qty = &MESH(analy).mat_disable_qty;
				   p_uc = MESH(analy).disable_material;

				   process_mat_obj_selection ( analy,  tokens, idx, token_cnt, mat_qty,
											   elem_qty, p_class,
											   p_elem, p_hide_qty, p_hide_elem_qty,
											   p_uc,
											   setval, include_selected, mat_selected );

				   /* We have excluded (meanind invis and disable by material type.  Now also go for the element
	 *                classes in case we want to bring them back by element class on at a time */

				  for( i = 0; i < MESH(analy).qty_class_selections; i++)
				  {
					  elem_qty = MESH(analy).by_class_select[i].p_class->qty;
					  p_hide_elem_qty = &MESH(analy).by_class_select[i].hide_class_elem_qty;
					  p_disable_elem_qty = &MESH(analy).by_class_select[i].disable_class_elem_qty;
					  p_elem = MESH(analy).by_class_select[i].hide_class_elem;
					  p_class = MESH(analy).by_class_select[i].p_class;

					  process_mat_obj_selection ( analy,  tokens, idx, token_cnt, mat_qty,
												  elem_qty, p_class,
												  p_elem, p_hide_qty, p_hide_elem_qty,
												  p_uc,
												  setval, include_selected, 0 );

					  p_elem = MESH(analy).by_class_select[i].disable_class_elem;

					  process_mat_obj_selection ( analy,  tokens, idx, token_cnt, mat_qty,
												  elem_qty, p_class,
												  p_elem, p_hide_qty, p_disable_elem_qty,
												  p_uc,
												  setval, include_selected, 0 );

				  }
			   } else
			   {
				   /* we are excluding/includeing by element class */
				  for(j = 1; j < token_cnt; j++)
				  {
					  if(isdigit(tokens[j][0]))
					  {
						  j--;
						  break;
					  }
				  }

				  elem_qty = MESH(analy).by_class_select[i].p_class->qty;
				  p_hide_elem_qty = &MESH(analy).by_class_select[i].hide_class_elem_qty;
				  p_disable_elem_qty = &MESH(analy).by_class_select[i].disable_class_elem_qty;
				  p_elem = MESH(analy).by_class_select[i].hide_class_elem;

				  process_mat_obj_selection ( analy,  tokens, j, token_cnt, mat_qty,
											  elem_qty, p_class,
											  p_elem, p_hide_qty, p_hide_elem_qty,
											  p_uc,
											  setval, include_selected, 0 );

				  p_elem = MESH(analy).by_class_select[i].disable_class_elem;

				  process_mat_obj_selection ( analy,  tokens, j, token_cnt, mat_qty,
											  elem_qty, p_class,
											  p_elem, p_hide_qty, p_disable_elem_qty,
											  p_uc,
											  setval, include_selected, 0 );

			   }



				if(class_selected == TRUE && strcmp(tokens[0], "include") == 0)
				{
					for(i = 1; i < token_cnt; i++)
					{
						if(isdigit(tokens[i][0]))
						{
							break;
						}
					}
					strcpy(tmp_tokens[0], tokens[0]);
					j = 1;
					for(; i < token_cnt; i++)
					{
						strcpy(tmp_tokens[j], tokens[i]);
						j++;
					}

					mat_qty = MESH( analy ).material_qty;
					p_hide_qty = &MESH( analy ). mat_hide_qty;
					p_uc = MESH( analy ).hide_material;
					process_mat_obj_selection ( analy,  tokens, 0, token_cnt, mat_qty,
												elem_qty, p_class,
												p_elem, p_hide_qty, p_hide_elem_qty,
												p_uc,
												setval, include_selected, TRUE );
					p_disable_qty = &MESH( analy ). mat_disable_qty;
					p_uc = MESH( analy ).disable_material;
					process_mat_obj_selection ( analy,  tokens, 0, token_cnt, mat_qty,
												elem_qty, p_class,
												p_elem, p_disable_qty, p_hide_elem_qty,
												p_uc,
												setval, include_selected, TRUE );

				}

			}


			if(!strcmp(tokens[0], "include") && !strcmp(tokens[1], "all"))
			{
				include_all_elements(analy);
			}


			process_node_selection ( analy );
			if ( analy->show_cut || analy->show_roughcut )
				parse_command( "on cut", analy );

	#ifdef DEBUG_SELECT
			printf("\n\n" );
			for ( i=0;
					i<mat_qty;
					i++ )
			{
				printf(" \n Material - %d", i );
				if ( MESH( analy ).brick_qty>0 )
					printf("\n\t[EXCLUDE] Brick Mat %d = %d/%d", i, MESH( analy ).hide_brick[i],       MESH( analy ).disable_brick[i] );
				if ( MESH( analy ).shell_qty>0 )
					printf("\n\t[EXCLUDE] Shell Mat %d = %d/%d", i, MESH( analy ).hide_shell[i],       MESH( analy ).disable_shell[i] );
				if ( MESH( analy ).beam_qty>0 )
					printf("\n\t[EXCLUDE] Beam Mat %d = %d/%d", i, MESH( analy ).hide_beam[i],         MESH( analy ).disable_beam[i]);
				if ( MESH( analy ).hide_particle )
					printf("\n\t[EXCLUDE] Particle Mat %d = %d/%d", i, MESH( analy ).hide_particle[i], MESH( analy ).disable_particle[i] );
				printf("\n\t[EXCLUDE] Mat %d = %d/%d", i, MESH( analy ).hide_material[i], MESH( analy ).disable_material[i] );
			}
	#endif
			reset_face_visibility( analy );
			if ( analy->dimension == 3 ) renorm = TRUE;
			analy->result_mod = TRUE;
			load_result( analy, TRUE, TRUE, TRUE );

			redraw = BINDING_MESH_VISUAL;
		}



		else if ( strcmp( tokens[0], "mtl" ) == 0 || strcmp( tokens[0], "matl") == 0 )
		{
			if( strcmp( tokens[0], "mtl") == 0 )
			{
				parse_single_command(&buf[4],analy);
			}
			else
			{
				parse_single_command(&buf[5],analy);
				//parse_mtl_cmd( analy, tokens, token_cnt, TRUE, &redraw, &renorm );
			}
		}
		else if ( strcmp( tokens[0], "surf" ) == 0 )
		{
			parse_surf_cmd( analy, tokens, token_cnt, TRUE, &redraw, &renorm );
		}
		else if ( strcmp( tokens[0], "tmx" ) == 0 )
		{
			p_mesh = MESH_P( analy );
			p_mesh->translate_material = TRUE;
			sscanf( tokens[1], "%d", &ival );
			sscanf( tokens[2], "%f", &val );
			if ( ival > 0 && ival <= p_mesh->material_qty )
				p_mesh->mtl_trans[0][ival-1] = val;
			reset_face_visibility( analy );
			renorm = TRUE;
			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "tmy" ) == 0 )
		{
			p_mesh = MESH_P( analy );
			p_mesh->translate_material = TRUE;
			sscanf( tokens[1], "%d", &ival );
			sscanf( tokens[2], "%f", &val );
			if ( ival > 0 && ival <= p_mesh->material_qty )
				p_mesh->mtl_trans[1][ival-1] = val;
			reset_face_visibility( analy );
			renorm = TRUE;
			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "tmz" ) == 0 )
		{
			p_mesh = MESH_P( analy );
			p_mesh->translate_material = TRUE;
			sscanf( tokens[1], "%d", &ival );
			sscanf( tokens[2], "%f", &val );
			if ( ival > 0 && ival <= p_mesh->material_qty )
				p_mesh->mtl_trans[2][ival-1] = val;
			reset_face_visibility( analy );
			renorm = TRUE;
			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "clrtm" ) == 0
				  || strcmp( tokens[0], "clrem" ) == 0 )
		{
			p_mesh = MESH_P( analy );
			p_mesh->translate_material = FALSE;
			for ( i = 0; i < p_mesh->material_qty; i++ )
				for ( j = 0; j < 3; j++ )
					p_mesh->mtl_trans[j][i] = 0.0;
			reset_face_visibility( analy );
			renorm = TRUE;
			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "emsph" ) == 0 )
		{
			if ( associate_matl_exp( token_cnt, tokens, analy, SPHERICAL ) != 0 )
				popup_dialog( USAGE_POPUP,
							  "emsph <x> <y> <z> <materials> [<name>]" );
		}
		else if ( strcmp( tokens[0], "emcyl" ) == 0 )
		{
			if ( associate_matl_exp( token_cnt, tokens, analy, CYLINDRICAL ) != 0 )
				popup_dialog( USAGE_POPUP,
							  "emcyl <x1> <y1> <z1> <x2> <y2> <z2> %s",
							  "<materials> [<name>]" );
		}
		else if ( strcmp( tokens[0], "emax" ) == 0 )
		{
			if ( associate_matl_exp( token_cnt, tokens, analy, AXIAL ) != 0 )
				popup_dialog( USAGE_POPUP,
							  "emax <x1> <y1> <z1> <x2> <y2> <z2> %s",
							  "<materials> [<name>]" );
		}
		else if ( strcmp( tokens[0], "em" ) == 0 )
		{
			if ( token_cnt < 2 )
				popup_dialog( USAGE_POPUP, "em [<name>...] <distance>" );
			else
			{
				MESH_P( analy )->translate_material = TRUE;
				explode_materials( token_cnt, tokens, analy, FALSE );
				reset_face_visibility( analy );
				renorm = TRUE;
				redraw = BINDING_MESH_VISUAL;
			}
		}
		else if ( strcmp( tokens[0], "emsc" ) == 0 )
		{
			if ( token_cnt < 2 )
				popup_dialog( USAGE_POPUP, "emsc [<name>...] <scale>" );
			else
			{
				MESH_P( analy )->translate_material = TRUE;
				explode_materials( token_cnt, tokens, analy, TRUE );
				reset_face_visibility( analy );
				renorm = TRUE;
				redraw = BINDING_MESH_VISUAL;
			}
		}
		else if ( strcmp( tokens[0], "tellem" ) == 0
				  || strcmp( tokens[0], "telem" ) == 0 )
		{
			char em_tokens[2][TOKENLENGTH] =
			{
				"tell", "em"
			};

			parse_tell_command( analy, em_tokens, 2, TRUE, &redraw );
		}
		else if ( strcmp( tokens[0], "emrm" ) == 0 )
		{
			remove_exp_assoc( token_cnt, tokens );
		}
		else if ( strcmp( tokens[0], "rnf" ) == 0 )
		{
			adjust_near_far( analy );
			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "near" ) == 0 )
		{
			if ( token_cnt == 2 && is_numeric_token( tokens[1] ) )
			{
				sscanf( tokens[1], "%f", &val );
				set_near( val );
				set_mesh_view();
				redraw = BINDING_MESH_VISUAL;
			}
			else
				popup_dialog( USAGE_POPUP, "near <value>" );
		}
		else if ( strcmp( tokens[0], "far" ) == 0 )
		{
			if ( token_cnt == 2 && is_numeric_token( tokens[1] ) )
			{
				sscanf( tokens[1], "%f", &val );
				set_far( val );
				set_mesh_view();
				redraw = BINDING_MESH_VISUAL;
			}
			else
				popup_dialog( USAGE_POPUP, "far <value>" );
		}
		else if ( strcmp( tokens[0], "lookfr" ) == 0 ||
				  strcmp( tokens[0], "lookat" ) == 0 ||
				  strcmp( tokens[0], "lookup" ) == 0 )
		{
			if ( token_cnt > 3 )
			{
				for ( i = 0; i < 3; i++ )
					sscanf( tokens[1+i], "%f", &pt[i] );

				if ( strcmp( tokens[0], "lookfr" ) == 0 )
					look_from( pt );
				else if ( strcmp( tokens[0], "lookat" ) == 0 )
					look_at( pt );
				else
					look_up( pt );

				adjust_near_far( analy );
				redraw = BINDING_MESH_VISUAL;
			}
			else
			{
				if ( strcmp( tokens[0], "lookup" ) == 0 )
					popup_dialog( USAGE_POPUP, "lookup <vx> <vy> <vz>" );
				else
					popup_dialog( USAGE_POPUP, "%s <x> <y> <z>", tokens[0] );
			}
		}
		else if ( strcmp( tokens[0], "tfx" ) == 0 )
		{
			if ( token_cnt == 2 )
			{
				sscanf( tokens[1], "%f", &val );
				move_look_from( 0, val );
				adjust_near_far( analy );
				redraw = BINDING_MESH_VISUAL;
			}
			else
				popup_dialog( USAGE_POPUP, "tfx <value>" );
		}
		else if ( strcmp( tokens[0], "tfy" ) == 0 )
		{
			if ( token_cnt == 2 )
			{
				sscanf( tokens[1], "%f", &val );
				move_look_from( 1, val );
				adjust_near_far( analy );
				redraw = BINDING_MESH_VISUAL;
			}
			else
				popup_dialog( USAGE_POPUP, "tfy <value>" );
		}
		else if ( strcmp( tokens[0], "tfz" ) == 0 )
		{
			if ( token_cnt == 2 )
			{
				sscanf( tokens[1], "%f", &val );
				move_look_from( 2, val );
				adjust_near_far( analy );
				redraw = BINDING_MESH_VISUAL;
			}
			else
				popup_dialog( USAGE_POPUP, "tfz <value>" );
		}
		else if ( strcmp( tokens[0], "tax" ) == 0 )
		{
			if ( token_cnt == 2 )
			{
				sscanf( tokens[1], "%f", &val );
				move_look_at( 0, val );
				adjust_near_far( analy );
				redraw = BINDING_MESH_VISUAL;
			}
			else
				popup_dialog( USAGE_POPUP, "tax <value>" );
		}
		else if ( strcmp( tokens[0], "tay" ) == 0 )
		{
			if ( token_cnt == 2 )
			{
				sscanf( tokens[1], "%f", &val );
				move_look_at( 1, val );
				adjust_near_far( analy );
				redraw = BINDING_MESH_VISUAL;
			}
			else
				popup_dialog( USAGE_POPUP, "tay <value>" );
		}
		else if ( strcmp( tokens[0], "taz" ) == 0 )
		{
			if ( token_cnt == 2 )
			{
				sscanf( tokens[1], "%f", &val );
				move_look_at( 2, val );
				adjust_near_far( analy );
				redraw = BINDING_MESH_VISUAL;
			}
			else
				popup_dialog( USAGE_POPUP, "taz <value>" );
		}
		else if ( strcmp( tokens[0], "camang" ) == 0 )
		{
			if ( token_cnt == 2 )
			{
				val = (float)strtod( tokens[1], (char **)NULL );
				set_camera_angle( val, analy );
				redraw = BINDING_MESH_VISUAL;
			}
			else
				popup_dialog( USAGE_POPUP, "camang <angle>" );
		}
		else if ( strcmp( tokens[0], "bbsrc" ) == 0 )
		{
			MO_class_data **mo_classes;

			p_mesh = MESH_P( analy );

			if ( token_cnt == 1 )
				valid_command = FALSE;
			else if ( strcmp( tokens[1], "nodes" ) == 0 )
			{
				mo_classes = NEW( MO_class_data *, "Bbox source class" );
				*mo_classes = p_mesh->node_geom;
				free( analy->bbox_source );
				analy->bbox_source = mo_classes;
				analy->bbox_source_qty = 1;
				analy->vis_mat_bbox = FALSE;
			}
			else if ( strcmp( tokens[1], "elems" ) == 0
					  || (rval = strcmp( tokens[1], "vis" )) == 0 )
			{
				mo_classes = NEW_N( MO_class_data *, p_mesh->elem_class_qty,
									"Bbox source classes" );
				ival = 0;

				for ( i = 0;
						i < sizeof( elem_sclasses ) / sizeof( elem_sclasses[0] );
						i++ )
				{
					p_lh = p_mesh->classes_by_sclass + elem_sclasses[i];

					if ( p_lh->qty != 0 )
					{
						p_classes = (MO_class_data **) p_lh->list;

						for ( j = 0; j < p_lh->qty; j++ )
						{
							mo_classes[ival] = p_classes[j];
							ival++;
						}
					}
				}
				free( analy->bbox_source );
				analy->bbox_source = mo_classes;
				analy->bbox_source_qty = p_mesh->elem_class_qty;
				if ( rval == 0 )
					analy->vis_mat_bbox = TRUE;
			}
			else
			{
				mo_classes = NEW_N( MO_class_data *, token_cnt - 1,
									"Bbox source classes" );
				for ( i = 1; i < token_cnt; i++ )
				{
					rval = htable_search( MESH( analy ).class_table, tokens[1],
										  FIND_ENTRY, &p_hte );
					if ( rval == OK )
						mo_classes[i - 1] = (MO_class_data *) p_hte->data;
					else
					{
						popup_dialog( INFO_POPUP,
									  "Class \"%s\" not known; command aborted.",
									  tokens[i] );
						valid_command = FALSE;
						free( mo_classes );
						break;
					}
				}

				if ( valid_command && i > 1 )
				{
					free( analy->bbox_source );
					analy->bbox_source = mo_classes;
					analy->bbox_source_qty = token_cnt - 1;
					analy->vis_mat_bbox = FALSE;
				}
			}

			if ( !valid_command )
				popup_dialog( USAGE_POPUP,
							  "bbsrc { nodes | elems | <elem class name(s)> | vis }" );
			else
				parse_command( "bbox", analy );
		}
		else if ( strcmp( tokens[0], "bbox" ) == 0 )
		{
			ival = analy->dimension;
			if ( token_cnt == 1 )
			{
				bbox_nodes( analy, analy->bbox_source, FALSE, pt, vec );
				if ( analy->keep_max_bbox_extent )
				{
					for ( i = 0; i < ival; i++ )
					{
						if ( pt[i] < analy->bbox[0][i] )
							analy->bbox[0][i] = pt[i];
						if ( vec[i] > analy->bbox[1][i] )
							analy->bbox[1][i] = vec[i];
					}
				}
				else
				{
					VEC_COPY( analy->bbox[0], pt );
					VEC_COPY( analy->bbox[1], vec );
				}
				redraw = BINDING_MESH_VISUAL;
			}
			else if ( token_cnt == 2 * ival + 1 )
			{
				for ( i = 0; i < ival; i++ )
				{
					pt[i] = atof( tokens[i + 1] );
					vec[i] = atof( tokens[i + 1 + ival] );
				}
				if ( pt[0] < vec[0] && pt[1] < vec[1]
						&& ( ival == 2 || ( pt[2] < vec[2] ) ) )
				{
					VEC_COPY( analy->bbox[0], pt );
					VEC_COPY( analy->bbox[1], vec );
					redraw = BINDING_MESH_VISUAL;
				}
			}

			if ( ival == 2 )
				analy->bbox[0][2] = analy->bbox[1][2] = 0.0;

			if ( !redraw )
				popup_dialog( USAGE_POPUP,
							  "bbox [<xmin> <ymin> [<zmin>] <xmax> <ymax> [<zmax>]]" );
			else
				set_view_to_bbox( analy->bbox[0], analy->bbox[1], ival );
		}
		else if ( strcmp( tokens[0], "plot" ) == 0 )
		{
			char original_tokens[MAXTOKENS][TOKENLENGTH];
			Result * res_ptr = NULL;
			Result * ptr;
			int cnt = token_cnt;
			int i, k;
			int versus =0;
			int start = -1;
			MO_class_data * p_class;
			int elem_class[14];

            /* If there are no states ignore request to plot */
            if ( analy->state_count == 0 )
            {
                // There are no states, so can't run plot command
                if ( previous_show_command == 0)
                {
                    popup_dialog(INFO_POPUP, "Ignoring requests to show as there are no states");
                    previous_show_command = 1;
                }
                return;
            }

			for(i = 0; i < 14; i++)
			{
				elem_class[i] = 0;
			}

			for(i = 0; i < MAXTOKENS; i++)
			{
				strcpy(original_tokens[i], "");
			}

			if(token_cnt > 1)
			{
				if(strstr(tokens[1], "in"))
				{
					analy->ref_surf = INNER;

				} else if(strstr(tokens[1], "out"))
				{
					analy->ref_surf = OUTER;

				} else if(strstr(tokens[1], "mid"))
				{
					analy->ref_surf = MIDDLE;
				}
			}
			for(i = 0; i < token_cnt; i++)
			{

				rval = htable_search(MESH(analy).class_table, tokens[i], FIND_ENTRY, &p_hte);
				if(rval == OK)
				{
					/* we found an element class */
					p_class = (MO_class_data *) p_hte->data;
					elem_class[p_class->superclass] = 1;
					start = i;
					break;
				}

			}
			/* Delete the plot for this result if computing an EI result */
			if ( analy->ei_result && strlen(analy->ei_result_name)>0 )
			{
				strcpy(tmp_token, "delth ");
				strcat(tmp_token, analy->ei_result_name);
				parse_command( tmp_token, analy );
			}

			if ( token_cnt>1 || check_for_result( analy, TRUE ) || analy->ei_result )
			{

				if(token_cnt > 1)
				{

					for(i = 1; i < token_cnt; i++)
					{
						rval = htable_search(MESH(analy).class_table, tokens[i], FIND_ENTRY, &p_hte);
						if(rval == OK && start == -1)
						{
							start = i;
						}
					}
					i = 1;
					int qty = 0;
					for(j = 0; j < token_cnt; j++)
					{
						strcpy(original_tokens[j], tokens[j]);
					}
					//WHY? --V
					for(; i < start; i++)
					{
						res_ptr = create_result_list(original_tokens[i], analy);
						for(ptr = res_ptr; ptr != NULL; ptr = ptr->next)
						{
							qty += ptr->qty;
						}
						delete_result_list(&res_ptr, analy);
					}

					j = 1;
					if(start > 1)
					{
					  /* we know the plot command line specifies at least one element class
	 *                   and that the value of start is the token index of the first element class*/
					  for(i = 1; i < start; i++)
					  {
						  res_ptr = create_result_list(original_tokens[i], analy);
						  for(ptr = res_ptr; ptr != NULL; ptr = ptr->next)
						  {
							  for(k = 0; k < ptr->qty; k++)
							  {
								  if(elem_class[ptr->superclasses[k]] == 1)
								  {
									  strcpy(tokens[j], ptr->original_name);
									  j++;
								  }
							  }
						  }
					  }
					  /* now finish adding in the element class specifications from the original tokens */
					  for(i = start; i < cnt; i++)
					  {
						  strcpy(tokens[j], original_tokens[i]);
						  j++;
					  }
					  token_cnt = j;
					} else
					{
						int qty = 0;
						for(i = 1; i < cnt; i++)
						{
							if(strcmp(original_tokens[i],"vs")==0)
							{
								qty++;
								strcpy(tokens[j],original_tokens[i]);
								j++;
							}
							res_ptr = create_result_list(original_tokens[i], analy);
							for(ptr = res_ptr; ptr != NULL; ptr = ptr->next)
							{
								qty += ptr->qty;
								strcpy(tokens[j], ptr->original_name);
								j++;
							}
						}
						if(qty > 0)
						{
							token_cnt = qty + 1;
						}
					}

					  i = 0;
					token_cnt = 0;
					while(strcmp(tokens[i], ""))
					{
						token_cnt++;
						i++;
					}
					cnt = token_cnt;
					for(i = 1; i < cnt; i++) {
						for(j = i+1; j < cnt; j++) {
							if(!strcmp(tokens[i], tokens[j])) {
								strcpy(tokens[j], "");
							}
						}
					}

					/* now adjust the token_cnt */
					for( i = 0; i < cnt; i++ ) {
						if(!strcmp(tokens[i], "")) {
							token_cnt--;
						}
					}

					/* final cleanup */
					for(i = 1; i < cnt; i++)
					{
						if(!strcmp(tokens[i], ""))
						{
							/* find the next token that is not the empty string */
							for(j = i+1; j < cnt; j++)
							{
								if(strcmp(tokens[j], ""))
								{
									strcpy(tokens[i], tokens[j]);
									strcpy(tokens[j], "");
									break;
								}
							}
						}
					}

					cnt = token_cnt;

					if(analy->cur_result != NULL && analy->cur_result->result_funcs[0] == load_primal_result)
					{
						for(i = 1; i < token_cnt; i++)
						{
							if(strstr(tokens[i], "sxy") && strstr(tokens[i], "es_") == NULL)
							{
								strcpy(tokens[i], "sxy");
							} else if(strstr(tokens[i], "syz") && strstr(tokens[i], "es_") == NULL)
							{
								strcpy(tokens[i], "syz");
							} else if(strstr(tokens[i], "szx") && strstr(tokens[i], "es_") == NULL)
							{
								strcpy(tokens[i], "szx");
							} else if(strstr(tokens[i], "sx") && strstr(tokens[i], "es_") == NULL)
							{
								strcpy(tokens[i], "sx");
							} else if(strstr(tokens[i], "sy") && strstr(tokens[i], "es_") == NULL)
							{
								strcpy(tokens[i], "sy");
							} else if(strstr(tokens[i], "sz") && strstr(tokens[i], "es_") == NULL)
							{
								strcpy(tokens[i], "sz");
							}
							if(strstr(tokens[i], "exy") && strstr(tokens[i], "es_") == NULL)
							{
								strcpy(tokens[i], "exy");
							} else if(strstr(tokens[i], "eyz") && strstr(tokens[i], "es_") == NULL)
							{
								strcpy(tokens[i], "eyz");
							} else if(strstr(tokens[i], "ezx") && strstr(tokens[i], "es_") == NULL)
							{
								strcpy(tokens[i], "ezx");
							} else if(strstr(tokens[i], "ex") && strstr(tokens[i], "es_") == NULL)
							{
								strcpy(tokens[i], "ex");
							} else if(strstr(tokens[i], "ey") && strstr(tokens[i], "es_") == NULL)
							{
								strcpy(tokens[i], "ey");
							} else if(strstr(tokens[i], "ez") && strstr(tokens[i], "es_") == NULL)
							{
								strcpy(tokens[i], "ez");
							}

						}
					}
				}
				else if(token_cnt == 1 &&
						  analy->cur_result != NULL &&
						  analy->cur_result->result_funcs[0] == load_primal_result)
				{
					i = 1;
					if(strstr(analy->cur_result->name, "sxy"))
					{
						strcpy(tokens[i], "sxy");
						token_cnt = 2;
					} else if(strstr(analy->cur_result->name, "syz"))
					{
						strcpy(tokens[i], "syz");
						token_cnt = 2;
					} else if(strstr(analy->cur_result->name, "szx"))
					{
						strcpy(tokens[i], "szx");
						token_cnt = 2;
					} else if(strstr(analy->cur_result->name, "sx"))
					{
						strcpy(tokens[i], "sx");
						token_cnt = 2;
					} else if(strstr(analy->cur_result->name, "sy"))
					{
						strcpy(tokens[i], "sy");
						token_cnt = 2;
					} else if(strstr(analy->cur_result->name, "sz"))
					{
						strcpy(tokens[i], "sz");
						token_cnt = 2;
					}
					if(strstr(analy->cur_result->name, "exy"))
					{
						strcpy(tokens[i], "exy");
						token_cnt = 2;
					} else if(strstr(analy->cur_result->name, "eyz"))
					{
						strcpy(tokens[i], "eyz");
						token_cnt = 2;
					} else if(strstr(analy->cur_result->name, "ezx"))
					{
						strcpy(tokens[i], "ezx");
						token_cnt = 2;
					} else if(strstr(analy->cur_result->name, "ex"))
					{
						strcpy(tokens[i], "ex");
						token_cnt = 2;
					} else if(strstr(analy->cur_result->name, "ey"))
					{
						strcpy(tokens[i], "ey");
						token_cnt = 2;
					} else if(strstr(analy->cur_result->name, "ez"))
					{
						strcpy(tokens[i], "ez");
						token_cnt = 2;
					}
					token_cnt += analy->cur_result->qty;
					for(i = 0; i < analy->cur_result->qty; i++)
					{
						if(analy->cur_result->original_names != NULL)
						{
							strcpy(tokens[i+1], analy->cur_result->original_names[i]);
						}
					}
					/* now remove duplicate token names */
					for(i = token_cnt; i > 0; i--)
					{
						for(j = 1; j < i; j++)
						{
							if(!strcmp(tokens[j], tokens[i]))
							{
								strcpy(tokens[i], "");
								token_cnt--;
								break;
							}
						}
					}
				}

				if(!strcmp(original_tokens[0], "") && analy->cur_result != NULL)
				{
					if(token_cnt == 1)
					{
						cnt = 2;
						strcpy(original_tokens[0], tokens[0]);
						strcpy(original_tokens[1], analy->cur_result->original_name);
					}
				}
				old_autoselect = analy->autoselect;

				Bool_type error_flag = FALSE;

				if((res_ptr != NULL && res_ptr->result_funcs[0] == load_primal_result) || (analy->cur_result != NULL &&
					analy->cur_result->result_funcs[0] == load_primal_result))
				{
					analy->autoselect = FALSE;
					create_plot_objects( token_cnt, tokens, analy, &analy->current_plots );
					analy->autoselect = old_autoselect;
				} else
				{
					//if we didnt find a result pointer
					if(res_ptr == NULL){
						//and our second argument does not match the current result
						if(!analy->cur_result &&
                           !analy->cur_result->name &&
                           strcmp(original_tokens[1], analy->cur_result->name)){
							error_flag = TRUE;
						}
					}
					if(!error_flag){
						analy->autoselect = FALSE;
						create_plot_objects( cnt, original_tokens, analy, &analy->current_plots );
						analy->autoselect = old_autoselect;
					}
				}
				if(!error_flag){
					redraw = BINDING_PLOT_VISUAL;
				}
				else{
					popup_dialog( INFO_POPUP, "Unknown result name entered" );
				}

			}
			analy->th_plotting = TRUE;
		}
		else if ( strcmp( tokens[0], "oplot" ) == 0 )
		{

            /* If there are no states ignore request to plot */
            if ( analy->state_count == 0 )
            {
                // There are no states, so can't run oplot command
                if ( previous_show_command == 0)
                {
                    popup_dialog(INFO_POPUP, "Ignoring requests to show as there are no states");
                    previous_show_command = 1;
                }
                return;
            }

			create_oper_plot_objects( token_cnt, tokens, analy,
									  &analy->current_plots );
			redraw = BINDING_PLOT_VISUAL;
			analy->th_plotting = TRUE;
		}
		else if ( strcmp( tokens[0], "timhis" ) == 0 )
		{
			parse_command( "plot", analy );
			/*valid_command = FALSE;*/
		}
		else if ( strcmp( tokens[0], "outth" ) == 0
				  || strcmp( tokens[0],  "thsave" ) == 0 )
		{
			if ( token_cnt > 1 )
				output_time_series( token_cnt, tokens, analy );
			else
				popup_dialog( USAGE_POPUP, "%s\n%s\n%s",
							  "outth <filename>",
							  "OR",
							  "outth <filename> [<result>...] [<mesh object>...]"
							  " [vs <result>]" );
		}
		else if ( strcmp( tokens[0], "delth" ) == 0 )
		{
			if ( token_cnt == 1 )
				popup_dialog( USAGE_POPUP, "%s\n%s\n%s",
							  "delth [<index> | <result> | <class short name>]...",
							  "OR",
							  "delth all" );
			else
			{
				delete_time_series( token_cnt, tokens, analy );
				redraw = redraw_for_render_mode( FALSE, RENDER_PLOT, analy );
			}
		}
		else if ( strcmp( tokens[0], "glyphqty" ) == 0 )
		{
			if ( token_cnt != 2 )
				popup_dialog( USAGE_POPUP, "%s\n%s\n%s", "glyphqty <qty>",
							  "Where qty indicates desired number of glyphs per curve,",
							  "and qty < 1 turns off glyph rendering." );
			else
			{
				set_glyphs_per_plot( atoi( tokens[1] ), analy );
				redraw = redraw_for_render_mode( FALSE, RENDER_PLOT, analy );
			}
		}
		else if ( strcmp( tokens[0], "glyphscl" ) == 0 )
		{
			if ( token_cnt != 2 )
				popup_dialog( USAGE_POPUP, "%s\n%s", "glyphscl <scale>"
							  "Where <scale> (floating point) scales glyph size." );
			else
			{
				set_glyph_scale( atof( tokens[1] ) );
				redraw = redraw_for_render_mode( FALSE, RENDER_PLOT, analy );
			}
		}
		else if ( strcmp( tokens[0], "mmloc" ) == 0 )
		{
			if ( token_cnt != 2
					|| ( ( tokens[1][0] != 'u' && tokens[1][0] != 'l' )
						 || ( tokens[1][1] != 'l' && tokens[1][1] != 'r' ) ) )
				popup_dialog( USAGE_POPUP, "%s\n%s\n%s",
							  "mmloc { ul | ll | lr | ur }",
							  "Where the first character dictates upper/lower and",
							  "the second character dictates left/right" );
			else
			{
				analy->minmax_loc[0] = tokens[1][0];
				analy->minmax_loc[1] = tokens[1][1];
				redraw = redraw_for_render_mode( FALSE, RENDER_PLOT, analy );
			}
		}
		/*
			else if ( strcmp( tokens[0], "mth" ) == 0 )
			{
				parse_mth_command( token_cnt, tokens, analy );
			}
		*/
		else if ( strcmp( tokens[0], "thsm" ) == 0 )
		{
			if ( token_cnt == 3 )
			{
				sscanf( tokens[1], "%d", &ival );
				analy->th_filter_width = ival;

				/* Filter type. */
				if ( strcmp( tokens[2], "box" ) == 0 )
					analy->th_filter_type = BOX_FILTER;
				/* Not implemented.
							else if ( strcmp( tokens[2], "tri" ) == 0 )
								analy->th_filter_type = TRIANGLE_FILTER;
							else if ( strcmp( tokens[2], "sync" ) == 0 )
								analy->th_filter_type = SYNC_FILTER;
				*/
				else
				{
					popup_dialog( WARNING_POPUP,
								  "Unknown filter type \"%s\"; using \"box\".\n",
								  tokens[2] );
					analy->th_filter_type = BOX_FILTER;
				}
			}
			else
				popup_dialog( USAGE_POPUP,
							  "thsm <filter_width> <filter_type>" );
		}
		else if ( strcmp( tokens[0], "gather" ) == 0 )
		{
			parse_gather_command( token_cnt, tokens, analy );
		}
		else if ( strcmp( tokens[0], "coordxf" ) == 0 )
		{
			setval = TRUE;

			if ( token_cnt == 10 )
			{
				for ( i = 1; i < 4; i++ )
					pt[i - 1] = atof( tokens[i] );
				for ( j = 0; j < 3; j++, i++ )
					pt2[j] = atof( tokens[i] );
				for ( j = 0; j < 3; j++, i++ )
					pt3[j] = atof( tokens[i] );
			}
			else if ( token_cnt == 4 )
			{
				for ( i = 0; i < 3; i++ )
					nodes[i] = atoi( tokens[i + 1] ) - 1;
				for ( i = 0; i < 3; i++ )
					pt[i] = analy->state_p->nodes.nodes3d[nodes[0]][i];
				for ( i = 0; i < 3; i++ )
					pt2[i] = analy->state_p->nodes.nodes3d[nodes[1]][i];
				for ( i = 0; i < 3; i++ )
					pt3[i] = analy->state_p->nodes.nodes3d[nodes[2]][i];
			}
			else
			{
				popup_dialog( USAGE_POPUP, "%s\n%s\n%s",
							  "coordxf <node 1> <node 2> <node 3>",
							  "OR",
							  "coordxf <x1> <y1> <z1> <x2> <y2> <z2> <x3> <y3> <z3>"
							);
				setval = FALSE;
			}

			if ( setval )
			{
				if ( analy->tensor_transform_matrix == NULL )
					analy->tensor_transform_matrix = (float (*)[3]) NEW_N( float, 9,
													 "Tensor xform matrix" );
				xfmat = analy->tensor_transform_matrix;

				/* "i" axis is direction from first to second point. */
				VEC_SUB( xfmat[0], pt2, pt );

				/* "j" axis is perpendicular to "i" and contains third point. */
				near_pt_on_line( pt3, pt, xfmat[0], pt4 );
				VEC_SUB( xfmat[1], pt3, pt4 );

				/* "k" axis is cross-product of "i" and "j". */
				VEC_CROSS( xfmat[2], xfmat[0], xfmat[1] );

				vec_norm( xfmat[0] );
				vec_norm( xfmat[1] );
				vec_norm( xfmat[2] );

				/* Want column vectors. */
				VEC_COPY( pt, xfmat[0] );
				VEC_COPY( pt2, xfmat[1] );
				VEC_COPY( pt3, xfmat[2] );
				for ( i = 0; i < 3; i++ )
				{
					xfmat[i][0] = pt[i];
					xfmat[i][1] = pt2[i];
					xfmat[i][2] = pt3[i];
				}

				if ( analy->do_tensor_transform )
				{
					analy->result_mod = TRUE;
					load_result( analy, TRUE, TRUE, FALSE );
					redraw = redraw_for_render_mode( FALSE, RENDER_MESH_VIEW,
													 analy );
				}
			}

		}
		else if ( strcmp( tokens[0], "dirv" ) == 0 )
		{
			if ( token_cnt != analy->dimension + 1 )
			{
				popup_dialog( USAGE_POPUP, "dirv <x> <y> <z (if 3D)>" );
				return;
			}

			for ( i = 0; i < analy->dimension; i++ )
				analy->dir_vec[i] = atof( tokens[i + 1] );

			if ( analy->dimension == 2 )
				analy->dir_vec[2] = 0.0;

			vec_norm( analy->dir_vec );

			if ( analy->cur_result != NULL
					&& strcmp( analy->cur_result->name, "pvmag" ) == 0 )
			{
				analy->result_mod = TRUE;
				load_result( analy, TRUE, TRUE, FALSE );
				redraw = redraw_for_render_mode( FALSE, RENDER_ANY, analy );
			}
		}
		else if ( strcmp( tokens[0], "dir3p" ) == 0 )
		{
			dim = analy->dimension;

			if ( dim == 2 )
			{
				popup_dialog( INFO_POPUP,
							  "\"dir3p\" not implemented for 2D data;\n%s",
							  "use \"dirv\" instead." );
				return;
			}

			if ( token_cnt != 10 )
			{
				popup_dialog( USAGE_POPUP, "dir3p <x1> <y1> <z1>  %s",
							  "<x2> <y2> <z2>  <x3> <y3> <z3>" );
				return;
			}

			for ( i = 0; i < dim; i++ )
				pt[i] = atof( tokens[i + 1] );
			for ( j = 0; j < dim; j++, i++ )
				pt2[j] = atof( tokens[i + 1] );
			for ( j = 0; j < dim; j++, i++ )
				vec[j] = atof( tokens[i + 1] );

			norm_three_pts( analy->dir_vec, pt, pt2, vec );

			if ( analy->cur_result != NULL
					&& strcmp( analy->cur_result->name, "pvmag" ) == 0 )
			{
				analy->result_mod = TRUE;
				load_result( analy, TRUE, TRUE, FALSE );
				redraw = redraw_for_render_mode( FALSE, RENDER_ANY, analy );
			}
		}
		else if ( strcmp( tokens[0], "dir3n" ) == 0 )
		{
			dim = analy->dimension;

			if ( dim == 2 )
			{
				popup_dialog( INFO_POPUP,
							  "\"dir3n\" not implemented for 2D data;\n%s",
							  "use \"dirv\" instead." );
				return;
			}

			if ( token_cnt != 4 )
			{
				popup_dialog( USAGE_POPUP, "dir3n <node 1> <node 2> <node 3>  %s" );
				return;
			}

			for ( i = 0; i < 3; i++ )
				nodes[i] = atoi( tokens[i + 1] ) - 1;

			for ( i = 0; i < 3; i++ )
				pt[i] = analy->state_p->nodes.nodes3d[nodes[0]][i];
			for ( i = 0; i < 3; i++ )
				pt2[i] = analy->state_p->nodes.nodes3d[nodes[1]][i];
			for ( i = 0; i < 3; i++ )
				vec[i] = analy->state_p->nodes.nodes3d[nodes[2]][i];

			norm_three_pts( analy->dir_vec, pt, pt2, vec );

			if ( analy->cur_result != NULL
					&& strcmp( analy->cur_result->name, "pvmag" ) == 0 )
			{
				analy->result_mod = TRUE;
				load_result( analy, TRUE, TRUE, FALSE );
				redraw = redraw_for_render_mode( FALSE, RENDER_ANY, analy );
			}
		}
		else if ( strcmp( tokens[0], "vec" ) == 0 )
		{
			/* Get the result types from the translation table. */
			for ( i = 1, j = 0; i < token_cnt && j < analy->dimension; i++, j++ )
			{
				if ( is_numeric_token( tokens[i] ) )
				{
					if ( atof( tokens[i] ) == 0.0 )
						analy->vector_result[j] = NULL;
				}
				else
				{
					if ( find_result( analy, ALL, TRUE, result_vector + j,
									  tokens[i] ) )
					{
						/* Save reference. */
						analy->vector_result[j] = result_vector + j;

						/* If found result is a vector or array... */
						if ( !result_vector[j].single_valued )
						{
							Bool_type did_it;
							int idx = j;
							Primal_result *p_pr = (Primal_result *)
												  result_vector[j].original_result;
							Result *p_dest = result_vector + j;

							/* ...use its components instead. */
							did_it = gen_component_results( analy, p_pr, &idx,
															analy->dimension,
															p_dest );
							if ( !did_it )
								popup_dialog( INFO_POPUP, "Unable to generate "
											  "component results \nfrom non-scalar"
											  " input result." );

							/* Load extracted component result references. */
							for ( ; j < idx; j++ )
								analy->vector_result[j] = result_vector + j;
						}
					}
					else
						popup_dialog( WARNING_POPUP,
									  "Result \"%s\" unrecognized, set to zero.\n",
									  tokens[i] );
				}
			}

			/* Any unspecified are set to zero. */
			for ( ;
					j < analy->dimension;
					j++ )
				analy->vector_result[j] = NULL;

			found = intersect_srec_maps( &analy->vector_srec_map,
										 analy->vector_result, 3, analy );

			if ( found )
			{
				if ( analy->show_vectors )
					update_vec_points( analy );

				/**/
				if ( analy->show_vectors /* || analy->show_carpet */ )
					redraw = redraw_for_render_mode( FALSE, RENDER_MESH_VIEW,
													 analy );

				if ( analy->cur_result != NULL
						&& strcmp( analy->cur_result->name, "pvmag" ) == 0 )
				{
					analy->result_mod = TRUE;
					load_result( analy, TRUE, TRUE, FALSE );
					redraw = redraw_for_render_mode( FALSE, RENDER_ANY,
													 analy );
				}
			}
			else
				popup_dialog( WARNING_POPUP, "%s\n%s\n%s",
							  "This database does not support the requested",
							  "combination of vector components anywhere on",
							  "the mesh." );
		}
		else if ( strcmp( tokens[0], "veccm" ) == 0 )
		{
			analy->vec_col_set = FALSE;
			analy->vec_hd_col_set = FALSE;
			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "vecscl" ) == 0 )
		{
			if ( token_cnt > 1 )
				sscanf( tokens[1], "%f", &analy->vec_scale );
			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "vhdscl" ) == 0 )
		{
			if ( token_cnt > 1 )
				sscanf( tokens[1], "%f", &analy->vec_head_scale );
			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "vgrid1" ) == 0 )
		{
			if ( token_cnt > 6 )
			{
				sscanf( tokens[1], "%d", &sz[0] );
				for ( i = 0; i < 3; i++ )
					sscanf( tokens[i+2], "%f", &pt[i] );
				for ( i = 0; i < 3; i++ )
					sscanf( tokens[i+5], "%f", &vec[i] );
				gen_vec_points( 1, sz, pt, vec, analy );
				analy->have_grid_points = TRUE;
			}
			redraw = redraw_for_render_mode( FALSE, RENDER_ANY, analy );
		}
		else if ( strcmp( tokens[0], "vgrid2" ) == 0 )
		{
			if ( token_cnt > 6 )
			{
				for ( i = 0; i < 2; i++ )
					sscanf( tokens[i+1], "%d", &sz[i] );
				for ( i = 0; i < 3; i++ )
					sscanf( tokens[i+3], "%f", &pt[i] );
				for ( i = 0; i < 3; i++ )
					sscanf( tokens[i+6], "%f", &vec[i] );
				gen_vec_points( 2, sz, pt, vec, analy );
				analy->have_grid_points = TRUE;
			}
			redraw = redraw_for_render_mode( FALSE, RENDER_ANY, analy );
		}
		else if ( strcmp( tokens[0], "vgrid3" ) == 0 )
		{
			if ( token_cnt > 6 )
			{
				for ( i = 0; i < 3; i++ )
					sscanf( tokens[i+1], "%d", &sz[i] );
				for ( i = 0; i < 3; i++ )
					sscanf( tokens[i+4], "%f", &pt[i] );
				for ( i = 0; i < 3; i++ )
					sscanf( tokens[i+7], "%f", &vec[i] );
				gen_vec_points( 3, sz, pt, vec, analy );
				analy->have_grid_points = TRUE;
			}
			redraw = redraw_for_render_mode( FALSE, RENDER_ANY, analy );
		}
		else if ( strcmp( tokens[0], "clrvgr" ) == 0 )
		{
			delete_vec_points( analy );
			redraw = redraw_for_render_mode( FALSE, RENDER_ANY, analy );
		}
	#ifdef HAVE_VECTOR_CARPETS
		else if ( strcmp( tokens[0], "carpet" ) == 0 )
		{
			if ( token_cnt > 1 )
				sscanf( tokens[1], "%f", &analy->vec_cell_size );
			if ( analy->show_carpet )
			{
				gen_carpet_points( analy );
				redraw = BINDING_MESH_VISUAL;
			}
		}
		else if ( strcmp( tokens[0], "regcar" ) == 0 )
		{
			if ( token_cnt > 1 )
				sscanf( tokens[1], "%f", &analy->reg_cell_size );
			if ( analy->show_carpet )
			{
				gen_reg_carpet_points( analy );
				redraw = BINDING_MESH_VISUAL;
			}
		}
		else if ( strcmp( tokens[0], "vecjf" ) == 0 )
		{
			if ( token_cnt > 1 )
				sscanf( tokens[1], "%f", &analy->vec_jitter_factor );
			if ( analy->show_carpet )
				redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "veclf" ) == 0 )
		{
			if ( token_cnt > 1 )
				sscanf( tokens[1], "%f", &analy->vec_length_factor );
			if ( analy->show_carpet )
				redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "vecif" ) == 0 )
		{
			if ( token_cnt > 1 )
				sscanf( tokens[1], "%f", &analy->vec_import_factor );
			if ( analy->show_carpet )
				redraw = BINDING_MESH_VISUAL;
		}
	#endif
		else if ( strcmp( tokens[0], "prake" ) == 0 )
		{
			if ( 3 == analy->dimension )
				prake( token_cnt, tokens, &ival, pt, vec, rgb, analy );
			else
				popup_dialog( INFO_POPUP, "Particle traces on 3D datasets only" );
		}
		else if ( strcmp( tokens[0], "clrpar" ) == 0 )
		{
			free_trace_points( analy );
			redraw = redraw_for_render_mode( FALSE, RENDER_ANY, analy );
		}
		else if ( strcmp( tokens[0], "ptrace" ) == 0
				  || strcmp( tokens[0], "aptrace" ) == 0 )
		{
			/* Start time. */
			if ( token_cnt > 1 )
				sscanf( tokens[1], "%f", &vec[0] );
			else
			{
				ival = 1;
				analy->db_query( analy->db_ident, QRY_STATE_TIME, &ival, NULL,
								 &val );
				vec[0] = val;
			}

			/* End time. */
			if ( token_cnt > 2 )
				sscanf( tokens[2], "%f", &vec[1] );
			else
			{
				max_state = get_max_state( analy );
				ival = max_state + 1;
				analy->db_query( analy->db_ident, QRY_STATE_TIME, &ival, NULL,
								 &val );
				vec[1] = val;
			}

			/* Time step size. */
			if ( token_cnt > 3 )
				sscanf( tokens[3], "%f", &val );
			else
				val = 0.0;

			/* "ptrace" deletes extant traces, leaving only new ones. */
			if ( tokens[0][0] == 'p' )
				free_trace_list( &analy->trace_pts );

			particle_trace( vec[0], vec[1], val, analy, FALSE );
			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "ptstat" ) == 0 )
		{
			if ( token_cnt < 4 )
				valid_command = FALSE;
			else
			{
				max_state = get_max_state( analy );

				switch( tokens[1][0] )
				{
				case 's':
					ival = atoi( tokens[2] );
					if ( ival < 1 || ival > max_state + 1 )
						valid_command = FALSE;
					else
						analy->db_query( analy->db_ident, QRY_STATE_TIME, &ival,
										 NULL, (void *) &vec[0] );
					break;
				case 't':
					val = atof( tokens[2] );
					i_args[0] = 2;
					i_args[1] = 1;
					i_args[2] = max_state + 1;
					analy->db_query( analy->db_ident, QRY_MULTIPLE_TIMES,
									 (void *) i_args, NULL, (void *) pt );
					if ( val < pt[0] || val > pt[1] )
						valid_command = FALSE;
					else
						vec[0] = val;
					break;
				default:
					valid_command = FALSE;
				}
			}

			if ( valid_command )
			{
				vec[1] = atof( tokens[3] );
				val = ( token_cnt == 5 ) ? atof( tokens[4] ) : 0.0;
				particle_trace( vec[0], vec[1], val, analy, TRUE );
				redraw = BINDING_MESH_VISUAL;
			}
			else
				popup_dialog( USAGE_POPUP, "%s\n%s\n%s",
							  "ptstat t <time> <duration> [<delta t>]",
							  " OR ",
							  "ptstat s <state> <duration> [<delta t>]" );
		}
		else if ( strcmp( tokens[0], "ptlim" ) == 0 )
		{
			ival = atoi( tokens[1] );
			analy->ptrace_limit = ( ival > 0 ) ? ival : 0;
			if ( analy->trace_pts != NULL )
				redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "ptwid" ) == 0 )
		{
			val = atof( tokens[1] );
			if ( val <= 0.0 )
			{
				popup_dialog( INFO_POPUP, "Trace width must be greater than 0.0." );
				valid_command = FALSE;
			}
			else
			{
				analy->trace_width = val;
				if ( analy->show_traces )
					redraw = BINDING_MESH_VISUAL;
			}
		}
		else if ( strcmp( tokens[0], "ptdiam" ) == 0 )
		{
			val = atof( tokens[1] );
			if ( val <= 0.0 )
			{
				popup_dialog( INFO_POPUP, "Point diameter must be greater than 0.0." );
				valid_command = FALSE;
			}
			else
			{
				analy->point_diam = val;
			}
		}
		else if ( strcmp( tokens[0], "ptdis" ) == 0 )
		{
			if ( analy->trace_disable != NULL )
			{
				free( analy->trace_disable );
				analy->trace_disable = NULL;
			}
			analy->trace_disable_qty = 0;

			if ( token_cnt > 1 )
			{
				int mtl_qty = MESH( analy ).material_qty;

				/* Store material numbers for disabling particle traces. */
				analy->trace_disable = NEW_N( int, token_cnt - 1, "Trace disable" );
				for ( i = 1, j = 0; i < token_cnt; i++ )
				{
					ival = atoi( tokens[i] );
					if ( ival > 0 && ival <= mtl_qty )
						analy->trace_disable[j++] = ival - 1;
					else
						popup_dialog( INFO_POPUP,
									  "Invalid material \"%d\" ignored", ival );
				}

				if ( j == 0 )
				{
					free( analy->trace_disable );
					analy->trace_disable = NULL;
				}
				else
					analy->trace_disable_qty = j;
			}

			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "outpt" ) == 0 )
		{
			if ( token_cnt == 2 )
				save_particle_traces( analy, tokens[1] );
			else
				popup_dialog( USAGE_POPUP, "outpt <filename>" );
		}
		else if ( strcmp( tokens[0], "inpt" ) == 0 )
		{
			if ( token_cnt == 2 )
				read_particle_traces( analy, tokens[1] );
			else
				popup_dialog( USAGE_POPUP, "inpt <filename>" );
		}
		else if ( strcmp( tokens[0], "maxst" ) == 0 )
		{
			if ( token_cnt == 2 )
			{
				if ( is_numeric_token( tokens[1] ) )
				{
					sscanf( tokens[1], "%d", &ival );
					ival--;
					analy->limit_max_state = TRUE;
					analy->db_query( analy->db_ident, QRY_QTY_STATES, NULL, NULL,
									 &j );
					j--;
					analy->max_state = MIN( ival, j );

					if ( analy->cur_state > analy->max_state )
					{
						analy->cur_state = analy->max_state;
						change_state( analy );
						redraw = redraw_for_render_mode( FALSE, RENDER_ANY,
														 analy );
					}

					if ( analy->render_mode == RENDER_PLOT )
					{
						/* Update plots for new extreme. */
						update_plot_objects( analy );
						redraw = redraw_for_render_mode( FALSE, RENDER_ANY,
														 analy );
					}
				}
				else if ( strcmp( tokens[1], "off" ) == 0 )
					analy->limit_max_state = FALSE;
				else
					valid_command = FALSE;
			}
			else
				valid_command = FALSE;

			if ( !valid_command )
				popup_dialog( USAGE_POPUP, "maxst 1 | 2 | 3 | ... | off" );
		}
		else if ( strcmp( tokens[0], "minst" ) == 0 )
		{
			if ( token_cnt == 2 )
			{
				if ( is_numeric_token( tokens[1] ) )
				{
					sscanf( tokens[1], "%d", &ival );
					ival--;
					analy->limit_min_state = TRUE;
					analy->min_state = MAX( ival, 0 );

					if ( analy->cur_state < analy->min_state )
					{
						analy->cur_state = analy->min_state;
						change_state( analy );
						redraw = redraw_for_render_mode( FALSE, RENDER_ANY,
														 analy );
					}

					if ( analy->render_mode == RENDER_PLOT )
					{
						/* Update plots for new extreme. */
						update_plot_objects( analy );
						redraw = redraw_for_render_mode( FALSE, RENDER_ANY,
														 analy );
					}
				}
				else if ( strcmp( tokens[1], "off" ) == 0 )
					analy->limit_min_state = FALSE;
				else
					valid_command = FALSE;
			}
			else
				valid_command = FALSE;

			if ( !valid_command )
				popup_dialog( USAGE_POPUP, "minst 1 | 2 | 3 | ... | off" );
		}
		else if ( strcmp( tokens[0], "refstate" ) == 0 )
		{
			if ( token_cnt == 1 )
			{
				popup_dialog( INFO_POPUP, "Current reference state is %d%s",
							  analy->reference_state,
							  analy->reference_state ?
							  "." : " (initial position)." );
			}
			else
			{
				ival = atoi( tokens[1] );
				old = analy->reference_state;
				set_ref_state( analy, ival );
				redraw = redraw_for_render_mode( FALSE, RENDER_ANY, analy );
				analy->result_mod = analy->check_mod_required( analy,
									REFERENCE_STATE, old,
									analy->reference_state );
				if ( analy->result_mod )
				{
					if ( analy->render_mode == RENDER_MESH_VIEW )
						load_result( analy, TRUE, TRUE, FALSE );
					analy->display_mode_refresh( analy );
				}
			}
		}
		else if ( strcmp( tokens[0], "stride" ) == 0 )
		{
			if ( token_cnt != 2 )
				valid_command = FALSE;
			else
			{
				ival = atoi( tokens[1] );
				if ( ival >= 1 && ival <= get_max_state( analy ) )
				{
					put_step_stride( ival );
					update_util_panel( STRIDE_EDIT, NULL );
				}
				else
					valid_command = FALSE;
			}
			if ( !valid_command )
				popup_dialog( USAGE_POPUP, "stride <step size>\n  Where\n"
							  "    1 <= <step size> < maximum state" );
		}
		else if ( strcmp( tokens[0], "state" ) == 0 ||
				  strcmp( tokens[0], "n" ) == 0 ||
				  strcmp( tokens[0], "p" ) == 0 ||
				  strcmp( tokens[0], "f" ) == 0 ||
				  strcmp( tokens[0], "l" ) == 0 )
		{
			max_state = get_max_state( analy );
			min_state = GET_MIN_STATE( analy );

			if ( strcmp( tokens[0], "state" ) == 0 )
			{
				if ( token_cnt < 2 )
					popup_dialog( USAGE_POPUP, "state <state number>" );
				else
				{
					sscanf( tokens[1], "%d", &i );
					i--;
					if ( (i != analy->cur_state && i <= max_state && i >= min_state) )
					{
						analy->cur_state = i;
					}
					else if ( i > max_state )
						popup_dialog( INFO_POPUP, "Maximum state is %d",
									  max_state + 1 );
					else if ( i < min_state )
						popup_dialog( INFO_POPUP, "Minimum state is %d",
									  min_state + 1 );
					else if ( i == analy->cur_state )
						popup_dialog( INFO_POPUP, "Already at state %d", i + 1 );
				}
			}
			else if ( strcmp( tokens[0], "n" ) == 0 || strcmp( tokens[0], "next" ) == 0 )
			{
				if ( analy->cur_state < max_state )
					analy->cur_state += 1;
				else
					popup_dialog( INFO_POPUP, "Already at max state..." );
			}
			else if ( strcmp( tokens[0], "p" ) == 0 )
			{
				ival = analy->cur_state + 1;
				analy->db_query( analy->db_ident, QRY_STATE_TIME, &ival, NULL,
								 &val );
				if ( analy->state_p->time <= val )
					if ( analy->cur_state > min_state )
						analy->cur_state -= 1;
					else
						popup_dialog( INFO_POPUP, "Already at min state..." );
			}
			else if ( strcmp( tokens[0], "f" ) == 0 || strcmp( tokens[0], "first" ) == 0)
				analy->cur_state = min_state;
			else if ( strcmp( tokens[0], "l" ) == 0 || strcmp( tokens[0], "last" ) == 0 )
				analy->cur_state = max_state;

			change_state( analy );
			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "time" ) == 0 )
		{
			if ( token_cnt == 2 )
			{
				max_state = get_max_state( analy );
				i_args[0] = 2;
				i_args[1] = GET_MIN_STATE( analy ) + 1;
				i_args[2] = max_state + 1;
				analy->db_query( analy->db_ident, QRY_MULTIPLE_TIMES,
								 (void *) i_args, NULL, (void *) pt );
				sscanf( tokens[1], "%f", &val );
				if ( val <= pt[0] )
				{
					analy->cur_state = i_args[1] - 1;
					change_state( analy );
				}
				else if ( val >= pt[1] )
				{
					analy->cur_state = max_state;
					change_state( analy );
				}
				else
					change_time( val, analy );

				redraw = BINDING_MESH_VISUAL;
			}
			else
				popup_dialog( USAGE_POPUP, "time <time value>" );
		}
		else if ( strcmp( tokens[0], "anim" ) == 0 )
		{
			env.animate_active   = TRUE;
			env.animate_reverse  = FALSE;
			redraw = BINDING_MESH_VISUAL;
			parse_animate_command( token_cnt, tokens, analy );
		}
		else if (strcmp( tokens[0], "animd" ) == 0)
		{
			env.animate_active =- TRUE;
			env.animate_reverse = FALSE;
			redraw = BINDING_MESH_VISUAL;
			parse_animate_command( token_cnt, tokens, analy );
		}
		else if ( strcmp( tokens[0], "animr" ) == 0 )
		{
			env.animate_active   = TRUE;
			env.animate_reverse  = TRUE;
			redraw = BINDING_MESH_VISUAL;
			parse_animate_command( token_cnt, tokens, analy );
		}
		else if ( strcmp( tokens[0], "animc" ) == 0 )
		{
			redraw = BINDING_MESH_VISUAL;
			continue_animate( analy );
		}
		else if ( strcmp( tokens[0], "stopan" ) == 0 || strcmp( tokens[0], "stopanim" ) == 0
				  || strcmp( tokens[0], "stop" ) == 0 )
		{
			end_animate_workproc( TRUE );
		}
		else if ( strcmp( tokens[0], "cutpln" ) == 0 || strcmp( tokens[0], "cutrpln" ) == 0 )
		{
			if ( token_cnt < 7 )
				popup_dialog( USAGE_POPUP,
							  "cutpln <pt x> <pt y> <pt z> %s",
							  "<norm x> <norm y> <norm z>" );
			else
			{
				Plane_obj *plane;

				plane = NEW( Plane_obj, "Cutting plane" );
				for ( i = 0; i < 3; i++ )
					sscanf( tokens[i+1], "%f", &(plane->pt[i]) );

				/* Cut reative plane - uses relative coords */
				if ( strcmp( tokens[0], "cutrpln" ) == 0 )
				{
					get_extents( analy, TRUE, TRUE, vert_low, vert_hi );
					for ( i = 0; i < 3; i++ )
					{
						sscanf( tokens[i+1], "%f", &(rel_pos[i]) );
						plane->pt[i] = vert_low[i] + (rel_pos[i] * (vert_hi[i] - vert_low[i]));
					}
				}
				for ( i = 0; i < 3; i++ )
					sscanf( tokens[i+4], "%f", &(plane->norm[i]) );

				vec_norm( plane->norm );
				APPEND( plane, analy->cut_planes );

				if ( analy->show_roughcut )
				{
					reset_face_visibility( analy );
					renorm = TRUE;
				}
				gen_cuts( analy );
	#ifdef HAVE_VECTOR_CARPETS
				gen_carpet_points( analy );
	#endif
				if ( analy->show_cut || analy->show_roughcut )
					redraw = BINDING_MESH_VISUAL;

				cutting_plane_defined = TRUE;
			}
		}
		else if ( strcmp( tokens[0], "clrcut" ) == 0 )
		{
			DELETE_LIST( analy->cut_planes );
			if ( analy->show_roughcut || analy->show_particle_cut )
			{
				reset_face_visibility( analy );
				analy->show_particle_cut = FALSE;
				renorm = TRUE;
			}

			for ( p_cl = analy->cut_poly_lists; p_cl != NULL; NEXT( p_cl ) )
			{
				Triangle_poly *tp_list = (Triangle_poly *) p_cl->list;
				DELETE_LIST( tp_list );
			}
			DELETE_LIST( analy->cut_poly_lists );

			if ( analy->show_cut || analy->show_roughcut )
				redraw = BINDING_MESH_VISUAL;

			cutting_plane_defined = FALSE;
			process_node_selection ( analy );
		}
		else if ( strcmp( tokens[0], "ison" ) == 0 )
		{
			if ( token_cnt > 1 )
				sscanf( tokens[1], "%d", &ival );
			else
				ival = 10;
			set_contour_vals( ival, analy );
			gen_contours( analy );
			gen_isosurfs( analy );
	#ifdef HAVE_VECTOR_CARPETS
			gen_carpet_points( analy );
	#endif
			if ( analy->show_isosurfs || analy->show_contours )
				redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "isop" ) == 0 )
		{
			if ( token_cnt > 1 )
			{
				sscanf( tokens[1], "%f", &val );
				add_contour_val( val, analy );
				gen_contours( analy );
				gen_isosurfs( analy );
	#ifdef HAVE_VECTOR_CARPETS
				gen_carpet_points( analy );
	#endif
				if ( analy->show_isosurfs || analy->show_contours )
					redraw = BINDING_MESH_VISUAL;
			}
		}
		else if ( strcmp( tokens[0], "isov" ) == 0 )
		{
			if ( token_cnt > 1 )
			{
				sscanf( tokens[1], "%f", &val );
				if ( analy->cur_result != NULL )
				{
					/*
					 * If units conversion is turned on, assume the user-
					 * specified value is in converted units and apply the
					 * inverse conversion so that the value is stored
					 * internally in "primal" units.
					 */
					if ( analy->perform_unit_conversion )
						val = (val - analy->conversion_offset)
							  / analy->conversion_scale;

					add_contour_val( (val - analy->result_mm[0]) /
									 (analy->result_mm[1] - analy->result_mm[0]),
									 analy );
				}
				gen_contours( analy );
				gen_isosurfs( analy );
	#ifdef HAVE_VECTOR_CARPETS
				gen_carpet_points( analy );
	#endif
				if ( analy->show_isosurfs || analy->show_contours )
					redraw = BINDING_MESH_VISUAL;
			}
		}
		else if ( strcmp( tokens[0], "clriso" ) == 0 )
		{
			if ( analy->contour_cnt > 0 )
			{
				free( analy->contour_vals );
				analy->contour_vals = NULL;
				analy->contour_cnt = 0;
			}
			free_contours( analy );
			DELETE_LIST( analy->isosurf_poly );
			if ( analy->show_isosurfs || analy->show_contours )
				redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "telliso" ) == 0
				  || strcmp( tokens[0], "teliso" ) == 0 )
		{
			char iso_tokens[2][TOKENLENGTH] =
			{
				"tell", "iso"
			};

			parse_tell_command( analy, iso_tokens, 2, TRUE, &redraw );
		}
		else if ( strcmp( tokens[0], "conwid" ) == 0 )
		{
			val = atof( tokens[1] );
			if ( val <= 0.0 )
			{
				popup_dialog( INFO_POPUP,
							  "Contour width must be greater than 0.0." );
				valid_command = FALSE;
			}
			else
			{
				analy->contour_width = val;
				if ( analy->show_contours )
					redraw = BINDING_MESH_VISUAL;
			}
		}
		else if ( strcmp( tokens[0], "sym" ) == 0 )
		{
			if ( token_cnt < 7 )
				popup_dialog( USAGE_POPUP,
							  "sym <pt-x> <pt-y> <pt-z> <norm-x> <norm-y> <norm-z>" );
			else
			{
				Refl_plane_obj *rplane;

				rplane = NEW( Refl_plane_obj, "Reflection plane" );
				for ( i = 0; i < 3; i++ )
					sscanf( tokens[i+1], "%f", &(rplane->pt[i]) );
				for ( i = 0; i < 3; i++ )
					sscanf( tokens[i+4], "%f", &(rplane->norm[i]) );

				/* Generate reflection transform mats. */
				create_reflect_mats( rplane );

				APPEND( rplane, analy->refl_planes );

				if ( analy->reflect )
					redraw = BINDING_MESH_VISUAL;
			}
		}
		else if ( strcmp( tokens[0], "clrsym" ) == 0 )
		{
			DELETE_LIST( analy->refl_planes );
			if ( analy->reflect )
				redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "inrgb" ) == 0 )
		{
			if ( token_cnt > 1 && token_cnt <= 3 )
			{
				setval = FALSE;

				if ( token_cnt == 3 )
					if ( strcmp( tokens[1], "bg" ) == 0 )
					{
						setval = TRUE;
						redraw = redraw_for_render_mode( FALSE, RENDER_ANY,
														 analy );
					}

				rgb_to_screen( tokens[token_cnt - 1], setval, analy );
			}
			else
				popup_dialog( USAGE_POPUP, "inrgb [bg] <rgb filename>" );
		}
		else if( strcmp( tokens[0], "outrgb" ) == 0 )
		{
			if ( token_cnt > 1 )
			{
				pushpop_window( PUSHPOP_ABOVE );
				screen_to_rgb( tokens[1], FALSE );
			}
			else
				popup_dialog( USAGE_POPUP, "outrgb <filename>" );
		}
		else if( strcmp( tokens[0], "outrgba" ) == 0 )
		{
			if ( token_cnt > 1 )
			{
				pushpop_window( PUSHPOP_ABOVE);
				screen_to_rgb( tokens[1], TRUE );
			}
			else
				popup_dialog( USAGE_POPUP, "outrgba <filename>" );
		}
		else if( strcmp( tokens[0], "outps" ) == 0 )
		{
			if ( token_cnt > 1 )
			{
				pushpop_window( PUSHPOP_ABOVE );
				screen_to_ps( tokens[1] );
			}
			else
				popup_dialog( USAGE_POPUP, "outps <filename>" );
		}
		else if( strcmp( tokens[0], "ldhmap" ) == 0 )
		{
			popup_dialog( INFO_POPUP, "%s\n%s\n%s",
						  "HDF palettes are no longer supported.",
						  "Convert HDF palettes with \"h2g\" utility,",
						  "then use \"ldmap\" instead." );
		}
		else if( strcmp( tokens[0], "ldmap" ) == 0 )
		{
			if ( token_cnt > 1 )
				read_text_colormap( tokens[1] );
			else
				popup_dialog( USAGE_POPUP, "ldmap <filename>" );
			redraw = BINDING_MESH_VISUAL;
		}
		else if( strcmp( tokens[0], "posmap" ) == 0 )
		{
			if ( token_cnt > 4 )
			{
				sscanf( tokens[1], "%f", &analy->colormap_pos[0] );
				sscanf( tokens[2], "%f", &analy->colormap_pos[1] );
				sscanf( tokens[3], "%f", &analy->colormap_pos[2] );
				sscanf( tokens[4], "%f", &analy->colormap_pos[3] );
				analy->use_colormap_pos = TRUE;
			}
			else
				popup_dialog( USAGE_POPUP, "%s\n%s",
							  "posmap <pos x> <pos y> <size x> <size y>",
							  "where <pos x> and <pos y> locate lower left corner" );
			redraw = BINDING_MESH_VISUAL;
		}
		else if( strcmp( tokens[0], "hotmap" ) == 0 )
		{
			hot_cold_colormap();
			redraw = BINDING_MESH_VISUAL;
		}
		else if( strcmp( tokens[0], "grmap" ) == 0 )
		{
			gray_colormap( FALSE );
			redraw = BINDING_MESH_VISUAL;
		}
		else if( strcmp( tokens[0], "igrmap" ) == 0 )
		{
			gray_colormap( TRUE );
			redraw = BINDING_MESH_VISUAL;
		}
		else if( strcmp( tokens[0], "invmap" ) == 0 )
		{
			invert_colormap();
			redraw = BINDING_MESH_VISUAL;
		}
		else if( strcmp( tokens[0], "coolmap" ) == 0 )
		{
			loadPresetCM("Cool");
			redraw = BINDING_MESH_VISUAL;
		}
		else if( strcmp( tokens[0], "hsvmap" ) == 0 )
		{
			loadPresetCM("Hsv");
			redraw = BINDING_MESH_VISUAL;
		}
		else if( strcmp( tokens[0], "jetmap" ) == 0 )
		{
			loadPresetCM("Jet");
			redraw = BINDING_MESH_VISUAL;
		}
		else if( strcmp( tokens[0], "prismmap" ) == 0 )
		{
			loadPresetCM("Prism");
			redraw = BINDING_MESH_VISUAL;
		}
		else if( strcmp( tokens[0], "wintermap" ) == 0 )
		{
			loadPresetCM("Winter");
			redraw = BINDING_MESH_VISUAL;
		}




		else if( strcmp( tokens[0], "conmap" ) == 0 ||
				 strcmp( tokens[0], "chmap" ) == 0 ||
				 strcmp( tokens[0], "cgmap" ) == 0 )
		{
			/* Load the smooth colormap first. */
			if ( strcmp( tokens[0], "chmap" ) == 0 )
				hot_cold_colormap();
			else if ( strcmp( tokens[0], "cgmap" ) == 0 )
				gray_colormap( FALSE );

			/* Now contour it. */
			if ( token_cnt > 1 )
				sscanf( tokens[1], "%d", &ival );
			else
				ival = 6;
			contour_colormap( ival );

			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "setcol" ) == 0 )
		{
			if ( token_cnt == 5 || token_cnt == 3 )
			{
				float *rgb_src;

				if ( strcmp( tokens[2], "default" ) == 0 )
					rgb_src = NULL;
				else
				{
					sscanf( tokens[2], "%f", &rgb[0] );
					sscanf( tokens[3], "%f", &rgb[1] );
					sscanf( tokens[4], "%f", &rgb[2] );
					rgb_src = rgb;
				}

				if ( strncmp( tokens[1], "plot", 4 ) == 0 )
				{
					ival = atoi( tokens[1] + 4 );
					if ( ival > 0 )
					{
						set_plot_color( ival, rgb_src );
						redraw = redraw_for_render_mode( FALSE, RENDER_PLOT,
														 analy );
					}
					else
						valid_command = FALSE;
				}
				else
				{
					set_color( tokens[1], rgb_src );

					if ( strcmp( tokens[1], "vec" ) == 0 )
						analy->vec_col_set = TRUE;
					else if ( strcmp( tokens[1], "vechd" ) == 0 )
						analy->vec_hd_col_set = TRUE;
					else if ( strcmp( tokens[1], "rmin" ) == 0
							  || strcmp( tokens[1], "rmax" ) == 0 )
						set_cutoff_colors( TRUE, analy->mm_result_set[0],
										   analy->mm_result_set[1] );

					if ( strcmp( tokens[1], "bg" ) == 0
							|| strcmp( tokens[1], "fg" ) == 0
							|| strcmp( tokens[1], "text" ) == 0 )
						/* These options affect either view mode. */
						redraw = redraw_for_render_mode( FALSE, RENDER_ANY, analy );
					else
						/* The other options affect only mesh view mode. */
						redraw = redraw_for_render_mode( FALSE, RENDER_MESH_VIEW,
														 analy );
				}
			}
			else
				valid_command = FALSE;

			if ( !valid_command )
				popup_dialog( USAGE_POPUP,
							  "%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s",
							  "setcol <which> <red> <green> <blue>",
							  "setcol <which> default\n",
							  "Where",
							  "  <which> is one of the following:",
							  "      fg  bg  text  mesh  edges  con  hilite"
							  "  plot<n>  rmax  rmin  select  vec  vechd;\n",
							  "  <red> <green> and <blue> are floating point "
							  "numbers on [0, 1];\n",
							  "  \"default\" instead of a color triple returns "
							  "<which> color to its initial value;\n",
							  "  <n> (from <which> = \"plot<n>\") is an integer "
							  "indicating a time history curve",
							  "      to set the color for (\"plot1\", \"plot2\", "
							  "etc.).  The integer is mod'ed with",
							  "      the quantity of colors stored "
							  "(currently 15) to generate the actual indexed ",
							  "      color entry to modify." );
		}
		else if ( strcmp( tokens[0], "savhis" ) == 0 )
		{
			if ( token_cnt > 1 )
				open_history_file( tokens[1] );
			else
				popup_dialog( USAGE_POPUP, "savhis <filename>" );

			/* Don't want to save _this_ command. */
			valid_command = FALSE;
		}
		else if ( strcmp( tokens[0], "endhis" ) == 0 )
		{
			close_history_file();
		}
		else if ( strcmp( tokens[0], "rdhis" ) == 0 ||
				  strcmp( tokens[0], "rdhist" ) == 0 ||
				  strcmp( tokens[0], "h" ) == 0 ||
				  strcmp( tokens[0], "hinclude" ) == 0 )
		{
			if ( token_cnt > 1 )
			{
				strcpy( analy->hist_filename, tokens[1] );
				analy->hist_line_cnt = 0;
				if(env.model_history_logging)
				{
					/* log the read history command in the default history file as a comment */
					/*strcpy(comment, "# ");
					strcat(comment, tokens[0]);
					strcat(comment, " ");
					strcat(comment, tokens[1]);
					strcat(comment, "\n");*/
					sprintf(comment, "# %s %s\n", tokens[0], tokens[1]);
					model_history_log_comment(comment, analy);
				}
				read_history_file( tokens[1], 1, 1, analy );
			}
			else
				popup_dialog( USAGE_POPUP, "%s\nOR\n%s", "rdhis <filename>",
							  "h <filename>" );
		}
		else if ( strcmp( tokens[0], "savhislog" ) == 0 )
		{
			if ( token_cnt > 1 )
				history_log_command( tokens[1] );
			else
				popup_dialog( USAGE_POPUP, "savhislog <filename>" );

			history_log_clear( );

			/* Don't want to save _this_ command. */
			valid_command = FALSE;
		}
		else if ( strcmp( tokens[0], "clrhislog" ) == 0 )
		{
			history_log_clear( );
			valid_command = FALSE;
		}
		else if ( strcmp( tokens[0], "loop" ) == 0 )
		{
	#ifdef SERIAL_BATCH
			/*
			 * Disable use of command "loop" to avoid "runaway processes"
			 * during batch operations
			 */

			popup_dialog( INFO_POPUP, "Command loop is disabled for batch execution." );
	#else
			loop_count = 1;
			if  ( token_cnt==3 )
				loop_count = atoi(tokens[2]);
			if ( token_cnt > 1 )
			{
				status = read_history_file( tokens[1], 1, loop_count, analy );
			}
			else
				popup_dialog( USAGE_POPUP, "loop <filename>" );
	#endif /* SERIAL_BATCH */
		}
		else if ( strcmp( tokens[0], "getedg" ) == 0 ||
				  strncmp( tokens[0], "getedge", 7 ) == 0 )
		{
			get_mesh_edges( analy->cur_mesh_id, analy );
			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "crease" ) == 0 )
		{
			sscanf( tokens[1], "%f", &val );
			crease_threshold = cos( (double)DEG_TO_RAD( 0.5 * val ) );
			explicit_threshold = cos( (double)DEG_TO_RAD( val ) );
			get_mesh_edges( analy->cur_mesh_id, analy );
			renorm = TRUE;
			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "edgwid" ) == 0 )
		{
			sscanf( tokens[1], "%f", &val );
			if ( token_cnt == 2 && val > 0.0 )
			{
				analy->edge_width = val;
				redraw = redraw_for_render_mode( FALSE, RENDER_MESH_VIEW, analy );
			}
			else
				popup_dialog( USAGE_POPUP, "edgwid <line width>" );
		}
		else if ( strcmp( tokens[0], "edgbias" )  == 0 ||
				  strcmp( tokens[0], "eb" )       == 0 ||
				  strcmp( tokens[0], "edgebias" ) == 0 )
		{
			if ( token_cnt != 2 )
				popup_dialog( USAGE_POPUP, "edgbias <bias value>" );
			else
			{
				sscanf( tokens[1], "%f", &val );
				analy->edge_zbias = val;
				redraw = BINDING_MESH_VISUAL;
			}
		}
		else if ( strcmp( tokens[0], "rzero" ) == 0 )
		{
			sscanf( tokens[1], "%f", &analy->zero_result );
			/*
			 * If units conversion is turned on, assume the user-specified
			 * value is in converted units and apply the inverse conversion
			 * so that the value is stored internally in "primal" units.
			 */
			if ( analy->perform_unit_conversion )
				analy->zero_result = (analy->zero_result
									  - analy->conversion_offset)
									 / analy->conversion_scale;
			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "rmax" ) == 0 )
		{
			sscanf( tokens[1], "%f", &analy->result_mm[1] );
			/*
			 * If units conversion is turned on, assume the user-specified
			 * value is in converted units and apply the inverse conversion
			 * so that the value is stored internally in "primal" units.
			 */
			if ( analy->perform_unit_conversion )
				analy->result_mm[1] = (analy->result_mm[1]
									   - analy->conversion_offset)
									  / analy->conversion_scale;
			analy->mm_result_set[1] = TRUE;
			set_cutoff_colors( TRUE,
							   analy->mm_result_set[0], analy->mm_result_set[1] );
			if ( analy->th_plotting )
				redraw = BINDING_PLOT_VISUAL;
			else
				redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "rmin" ) == 0 )
		{
			sscanf( tokens[1], "%f", &analy->result_mm[0] );
			/*
			 * If units conversion is turned on, assume the user-specified
			 * value is in converted units and apply the inverse conversion
			 * so that the value is stored internally in "primal" units.
			 */
			if ( analy->perform_unit_conversion )
				analy->result_mm[0] = (analy->result_mm[0]
									   - analy->conversion_offset)
									  / analy->conversion_scale;
			analy->mm_result_set[0] = TRUE;
			set_cutoff_colors( TRUE,
							   analy->mm_result_set[0], analy->mm_result_set[1] );

			if ( analy->th_plotting )
				redraw = BINDING_PLOT_VISUAL;
			else
				redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "tmin" ) == 0 ||
				  strcmp( tokens[0], "mint" ) == 0 ) /* Time Min - used for TH Plots */
		{
			sscanf( tokens[1], "%f", &analy->time_mm[0] );
			analy->mm_time_set[0] = TRUE;

			redraw = redraw_for_render_mode( FALSE, RENDER_ANY,
											 analy );
		}
		else if ( strcmp( tokens[0], "tmax" ) == 0 ||
				  strcmp( tokens[0], "maxt" ) == 0 ) /* Time Max - used for TH Plots */
		{
			sscanf( tokens[1], "%f", &analy->time_mm[1] );
			analy->mm_time_set[1] = TRUE;

	#ifdef TMAX
			int i=0;
			int state_num=1, qty_states=1;
			Int_2tuple st_nums;
			float *times;
			st_nums[0] = 1;
			st_nums[1] = analy->last_state + 1;
			qty_states = st_nums[1] - st_nums[0] + 1;
			times = NEW_N( float, qty_states, "Time series time array" );
			analy->db_query( analy->db_ident, QRY_SERIES_TIMES, (void *) st_nums, NULL,
							 (void *) times );
			for( i=0;
					i<qty_states;
					i++ )
			{
				if ( analy->time_mm[1]== times[i] )
				{
					char cmd[64];
					state_num = i+1;
					sprintf( cmd, "maxst %d", state_num );
					parse_command( cmd, analy);
					break;
				}
			}
	#endif
			redraw = redraw_for_render_mode( FALSE, RENDER_ANY,
											 analy );
		}
		else if ( strcmp( tokens[0], "clrthr" ) == 0 )
		{
			set_cutoff_colors( FALSE,
							   analy->mm_result_set[0], analy->mm_result_set[1] );
			analy->zero_result = 0.0;
			analy->mm_result_set[0] = FALSE;
			analy->mm_result_set[1] = FALSE;

			if ( analy->mm_time_set[0] )
				parse_command( "minst 1", analy);
			if ( analy->mm_time_set[1] )
			{
				char cmd[64];
				sprintf( cmd, "maxst %d", analy->last_state + 1 );
				parse_command( cmd, analy);
			}

			analy->mm_time_set[0]   = FALSE;
			analy->mm_time_set[1]   = FALSE;
			if ( analy->use_global_mm )
			{
				analy->result_mm[0] = analy->global_mm[0];
				analy->result_mm[1] = analy->global_mm[1];
			}
			else
			{
				analy->result_mm[0] = analy->state_mm[0];
				analy->result_mm[1] = analy->state_mm[1];
			}
			if ( analy->th_plotting )
				redraw = BINDING_PLOT_VISUAL;
			else
				redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "globmm" ) == 0 )
		{
			get_global_minmax( analy );
			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "extreme_min" ) == 0 ||
				  strcmp( tokens[0], "exmin" ) == 0 )
		{
			view_state = 0;
			if ( token_cnt > 1 )
			{
				sscanf( tokens[1], "%d", &view_state );
			}
			analy->result_mod = TRUE;
			get_extreme_minmax( analy, EXTREME_MIN, view_state);

			reset_face_visibility( analy );
			if ( analy->dimension == 3 ) renorm = TRUE;
			redraw = BINDING_MESH_VISUAL;
			analy->extreme_min = TRUE;
			analy->extreme_max = FALSE;
		}
		else if ( strcmp( tokens[0], "extreme_max" ) == 0 ||
				  strcmp( tokens[0], "exmax" ) == 0 )
		{
			view_state = 0;
			if ( token_cnt > 1 )
			{
				sscanf( tokens[1], "%d", &view_state );
			}
			analy->result_mod = TRUE;
			get_extreme_minmax( analy, EXTREME_MAX, view_state);

			reset_face_visibility( analy );
			if ( analy->dimension == 3 ) renorm = TRUE;
			redraw = BINDING_MESH_VISUAL;
			analy->extreme_min = FALSE;
			analy->extreme_max = TRUE;
		}
		else if ( strcmp( tokens[0], "resetmm" ) == 0 )
		{
			redraw = reset_global_minmax( analy );
			if ( redraw != NO_VISUAL_CHANGE && analy->extreme_result == FALSE )
				load_result( analy, TRUE, TRUE, FALSE );
		}
		else if ( strcmp( tokens[0], "outmm" ) == 0 )
		{
			if ( token_cnt < 2 )
				popup_dialog( USAGE_POPUP, "outmm <file name> [<mat>...]" );
			else
				redraw = parse_outmm_command( analy, tokens, token_cnt );
		}
		else if ( strcmp( tokens[0], "break" ) == 0 )
		{
			analy->hist_paused = TRUE;
		}
		else if ( strcmp( tokens[0], "pause" ) == 0 )
		{

	#ifdef SERIAL_BATCH

			/*
			 * pausing is redundant in batch mode
			 *
			 * Pead specified "pause" duration token to properly
			 * continue GRIZ processing
			 */

			if ( token_cnt > 1 )
				sscanf( tokens[1], "%d", &ival );

	#else
			if ( token_cnt > 1 )
			{
				sscanf( tokens[1], "%d", &ival );
				sleep( ival );
			}
			else
			{
				analy->hist_paused = TRUE;
			}
	#endif
		}
		else if ( strcmp( tokens[0], "dellit" ) == 0 )
		{
			delete_lights();
			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "continue" ) == 0 ||
				  strcmp( tokens[0], "go" ) == 0       ||
				  strcmp( tokens[0], "resume" ) == 0 )
		{
			if ( analy->hist_line_cnt>1 && env.history_input_active )
				read_history_file( analy->hist_filename, analy->hist_line_cnt, 1, analy );

			analy->hist_paused = FALSE;

			if ( env.animate_active )
			{
				redraw = BINDING_MESH_VISUAL;
				continue_animate( analy );
			}
		}
		else if ( strcmp( tokens[0], "light" ) == 0 )
		{
			if ( token_cnt >= 6 )
			{
				set_light( token_cnt, tokens );
				redraw = BINDING_MESH_VISUAL;
			}
			else
				popup_dialog( USAGE_POPUP,
							  "light n x y z w [amb r g b] [diff r g b]\n%s%s",
							  "      [spec r g b] [spotdir x y z] ",
							  "[spot exp ang]" );
		}
		else if ( strcmp( tokens[0], "tlx" ) == 0 )
		{
			sscanf( tokens[1], "%d", &ival );
			sscanf( tokens[2], "%f", &val );
			move_light( ival-1, 0, val );
			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "tly" ) == 0 )
		{
			sscanf( tokens[1], "%d", &ival );
			sscanf( tokens[2], "%f", &val );
			move_light( ival-1, 1, val );
			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "tlz" ) == 0 )
		{
			sscanf( tokens[1], "%d", &ival );
			sscanf( tokens[2], "%f", &val );
			move_light( ival-1, 2, val );
			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "mat" ) == 0 )
		{
			//redraw = set_material( token_cnt, tokens, analy->max_mesh_mat_qty );
			//parse_mtl_cmd( analy, tokens, token_cnt, FALSE, &redraw, &renorm );
		    static GLfloat *save_props[MTL_PROP_QTY];
			parse_embedded_mtl_cmd( analy, tokens, token_cnt, FALSE, save_props, &renorm );
			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "apply" ) == 0 ){
			//activate preview mode
			analy->preview_mode = False;
			analy->defaultColorActive = False;
			analy->lastColorActive = False;
			//pas on anything else
			if(token_cnt > 1){
				parse_single_command(&buf[6], analy);
			}
		}
		else if ( strcmp( tokens[0], "preview" ) == 0 ){
			//activate preview mode
			analy->preview_mode = True;
			//save current colors to last
			backup_current_colors(analy,False);
			//save current colors to default if not active
			if(!analy->defaultColorActive){
				backup_current_colors(analy,True);
			}
			//
			if(token_cnt > 1){
				parse_single_command(&buf[8], analy);
			}
		}
		else if ( strcmp( tokens[0], "cancel" ) == 0 ){
			//restore last colors
			restore_color(analy,False);
			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "default" ) == 0 ){
			//restore default set of colors
			restore_color(analy,True);
			//deactivate preview mode
			analy->preview_mode = False;
			//turn off both sets active flag
			analy->defaultColorActive = False;
			analy->lastColorActive = False;

			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "alias" ) == 0 )
		{
			create_alias( token_cnt, tokens );
		}
		else if ( strcmp( tokens[0], "unalias" ) == 0 )
		{
			delete_alias( token_cnt, tokens );
		}
		else if ( strcmp( tokens[0], "listalias" ) == 0 )
		{
			list_alias( token_cnt, tokens );
		}
		/*
			else if ( strcmp( tokens[0], "resttl" ) == 0 )
			{
				ival = lookup_result_id( tokens[1] );
				if ( ival < 0 )
					wrt_text( "Result unrecognized: %s\n", tokens[1] );
				else
				{
					i = resultid_to_index[ival];
					trans_result[i][1] =
							NEW_N( char, strlen(tokens[2])+1, "Result title" );
					strcpy( trans_result[i][1], tokens[2] );
					if ( analy->result_id == ival )
						strcpy( analy->result_title, tokens[2] );
					redraw = redraw_for_render_mode( FALSE, RENDER_ANY, analy );
				}
			}
		*/
		else if ( strcmp( tokens[0], "title" ) == 0 )
		{
			/* analy->title is a hard coded char array of size G_MAX_STRING_LEN which is 512 */
			/* first lets find out the size of the string to copy which depends on the token count */
			n = 0;
			for(i = 0; i < token_cnt; i++)
			{
				n += strlen(tokens[i]) + 1;
				/* account for a single space between tokens if the token count > 2 */
				if(token_cnt > 2 && i < token_cnt - 1)
				{
					n++;
				}
			}
			/* now n has the correct value for strcpy that is guaranteed not to cause a buffer overflow */
			if(n < G_MAX_STRING_LEN)
			{
				strcpy(analy->title, tokens[1]);

				for(i = 2; i < token_cnt; i++)
				{
					strcat(analy->title, " ");
					strcat( analy->title, tokens[i] );
				}

			}
			else
			{
				if(strlen(tokens[1]) < G_MAX_STRING_LEN)
				{
					strcpy(analy->title, tokens[1]);
				}
				else
				{
					popup_dialog(USAGE_POPUP, "The string enter for the title is too long, try something that will fit on one line.");
				}

				for(i = 2; i < token_cnt; i++)
				{
					if(strlen(analy->title) + strlen(tokens[i]) < G_MAX_STRING_LEN)
					{
						strcpy(analy->title, tokens[i]);
					}
				}

			}


			if ( analy->show_title )
				redraw = redraw_for_render_mode( FALSE, RENDER_ANY, analy );
		}
		else if ( strcmp( tokens[0], "echo" ) == 0 )
		{
			char echo_buf[521];
			int k=0;
			strcpy( echo_buf, tokens[1] );
			if ( token_cnt>2 )
			{
				for ( k=2;
						k<token_cnt;
						k++ )
				{
					strcat( echo_buf, " " );
					strcat( echo_buf, tokens[k] );
				}
			}
			wrt_text( "%s\n\n", echo_buf );
		}
		else if ( strcmp( tokens[0], "vidti" ) == 0 )
		{
			sscanf( tokens[1], "%d", &ival );
			set_vid_title( ival-1, tokens[2] );
		}
		else if ( strcmp( tokens[0], "vidttl" ) == 0 )
		{
			draw_vid_title();
		}
		else if ( strcmp( tokens[0], "dscala") == 0 ||
				  strcmp( tokens[0], "dscalc") == 0 )
		{
			char cmd[512];
			char scale[32];
			char out_command[32];
			char out_base_name[256];
			char out_file_name[256];
			int reps = 1;
			int i, j;
			float incr;
			float amplification = 1.0;
			float num_divisions;
			float dmax;
			int output_file = 0;
			out_command[0] = '\0';
			parse_command("on dscal", analy);
			Bool_type cflag = FALSE;
			if (strcmp( tokens[0], "dscalc") == 0) {
				cflag = TRUE;
			}
			if(token_cnt > 1)
			{
				sscanf(tokens[1], "%f", &num_divisions);
				if( num_divisions < 1.0)
				{
					popup_dialog(USAGE_POPUP, " number of divisions specified must be greater than 1.0\n");
					return;
				}
				if(token_cnt > 2)
				{
					sscanf(tokens[2], "%d", &reps);
					if(reps < 0)
					{
						popup_dialog(USAGE_POPUP, "The number of cycles must be > 1\n");
						return;
					}
					if(token_cnt > 3)
					{
						sscanf(tokens[3], "%f", &amplification);
						if(amplification < 0)
						{
							popup_dialog(USAGE_POPUP, "The amplification must be > 1\n");
							return;
						}
						if(token_cnt > 4)
						{
							strcpy(out_command, tokens[4]);
							if (strcmp(out_command,"outjpeg") != 0 &&
								strcmp(out_command,"outpng") != 0)
							{
								popup_dialog(USAGE_POPUP, "The current supported output is outjpeg");
								return;
							}
							output_file = 1;
							if(token_cnt >5)
							{
								strcpy(out_base_name, tokens[5]);
							}else
							{
								strcpy(out_base_name, analy->root_name);
							}
						}
					}
				}
			}
			incr = (2*PI)/num_divisions;
			int digits = 0;
			int tempDiv = num_divisions;
			while(tempDiv){
				tempDiv /= 10;
				digits++;
			}
			//digits = ceil(log10(num_divisions));
			for(i = 0; i < reps; i++)
			{
				dmax = 0.0;
				for(j = 0; j <= num_divisions; j++)
				{
					if(cflag){
						sprintf(scale, "%f", (amplification*cos(dmax)));
					}
					else{
						sprintf(scale, "%f", (amplification*sin(dmax)));
					}
					strcpy(cmd, "dscal ");
					strcat(cmd, scale);
					parse_command(cmd, analy);
					if(output_file)
					{
						int index = (j+(i*num_divisions))+1;
						sprintf(out_file_name, "%s-01_%0*d.jpg", out_base_name, digits, index);
						strcpy(cmd, out_command);
						strcat(cmd, " ");
						strcat(cmd, out_file_name);
						parse_command(cmd, analy);
					}
					dmax = dmax + incr;//1.0 - ( * sin(2*PI*j*incr));
				}
			}
			VEC_SET( analy->displace_scale, amplification, amplification, amplification )
		}
		else if ( strcmp( tokens[0], "dscal" ) == 0 ||
				  strcmp( tokens[0], "dscale" ) == 0 )
		{
			analy->displace_exag = TRUE;
			sscanf( tokens[1], "%f", &val );
			VEC_SET( analy->displace_scale, val, val, val );
			if ( val == 1.0 )
				analy->displace_exag = FALSE;
			else
				analy->displace_exag = TRUE;
			renorm = ( analy->dimension == 3 );
			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "dscalx" ) == 0 )
		{
			sscanf( tokens[1], "%f", &val );
			analy->displace_scale[0] = val;
			analy->displace_exag = TRUE;
			renorm = ( analy->dimension == 3 );
			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "dscaly" ) == 0 )
		{
			sscanf( tokens[1], "%f", &val );
			analy->displace_scale[1] = val;
			analy->displace_exag = TRUE;
			renorm = ( analy->dimension == 3 );
			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "dscalz" ) == 0
				  && analy->dimension == 3 )
		{
			sscanf( tokens[1], "%f", &val );
			analy->displace_scale[2] = val;
			analy->displace_exag = TRUE;
			renorm = TRUE;
			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "exec" ) == 0 )
		{
			if ( token_cnt > 1 )
				system( tokens[1] );
		}
		else if ( strcmp( tokens[0], "inslp" ) == 0 )
		{
			if ( token_cnt > 1 )
				read_slp_file( tokens[1], analy );
			else
				popup_dialog( USAGE_POPUP, "inslp <filename>" );
		}
		else if ( strcmp( tokens[0], "inref" ) == 0 )
		{
			if ( token_cnt > 1 )
				read_ref_file( tokens[1], analy );
			else
				popup_dialog( USAGE_POPUP, "inref <filename>" );
		}
		/*
			else if ( strcmp( tokens[0], "genref" ) == 0 )
			{
				if ( tokens[1][0] == 'x' || tokens[1][0] == 'X' )
					ival = 0;
				if ( tokens[1][0] == 'y' || tokens[1][0] == 'Y' )
					ival = 1;
				else
					ival = 2;

				sscanf( tokens[2], "%f", &val );

				gen_ref_from_coord( analy, ival, val );
			}
			else if ( strcmp( tokens[0], "refave" ) == 0 )
			{
				val = get_ref_average_area( analy );
				if ( val == 0.0 )
					wrt_text( "No reference faces present!\n" );
				else
				{
					wrt_text( "Average reference face area: %f\n", val );
					wrt_text( "1 / average reference face area: %f\n", 1.0/val );
				}
			}
			else if ( strcmp( tokens[0], "triave" ) == 0 )
			{
				val = get_tri_average_area( analy );
				if ( val == 0.0 )
					wrt_text( "No triangles present!\n" );
				else
				{
					wrt_text( "Average triangle area: %f\n", val );
					wrt_text( "1 / average triangle area: %f\n", 1.0/val );
				}
			}
		*/
		else if ( strcmp( tokens[0], "clrref" ) == 0 )
		{
			DELETE_LIST( analy->ref_polys );
			if ( analy->show_ref_polys )
				redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "outref" ) == 0 )
		{
			write_ref_file( tokens, token_cnt, analy );
		}
		else if ( strcmp( tokens[0], "outobj" ) == 0 )
		{
			if ( token_cnt > 1 )
				write_obj_file( tokens[1], analy );
			else
				popup_dialog( USAGE_POPUP, "outobj <filename>" );
		}
		else if ( strcmp( tokens[0], "outhid" ) == 0 )
		{
			if ( token_cnt > 1 )
			{

				write_hid_file( tokens[1], analy );

				/* Added: IRC - 10/12/04: Call the inline hidden line
				 * function to generate the output PS file.
				 * See SCR#:289
				 */
				popup_dialog( INFO_POPUP,
							  "Generating hidden line output in file %s.ps",
							  tokens[1] );
				hidden_inline( tokens[1] );
			}
			else
				popup_dialog( USAGE_POPUP, "outhid <filename>" );
		}
		else if ( strcmp( tokens[0], "outgeom" ) == 0 )
		{
			if ( token_cnt > 1 )
			{

				write_geom_file( tokens[1], analy, FALSE );

				popup_dialog( INFO_POPUP,
							  "Generating mesh geometry output in file %s",
							  tokens[1] );
			}
			else
				popup_dialog( USAGE_POPUP, "outgeom <filename>" );
		}
		else if ( strcmp( tokens[0], "outsgeom" ) == 0 )
		{
			if ( token_cnt > 1 )
			{

				write_geom_file( tokens[1], analy, TRUE );

				popup_dialog( INFO_POPUP,
							  "Generating mesh surface geometry output in file %s",
							  tokens[1] );
			}
			else
				popup_dialog( USAGE_POPUP, "outgeom <filename>" );
		}

	#ifdef PNG_SUPPORT
		else if ( strcmp( tokens[0], "outpng" ) == 0 )
		{
			pushpop_window( PUSHPOP_ABOVE );
			if ( token_cnt > 1 )
				write_PNG_file( tokens[1], FALSE, png_compression_level );
			else
				popup_dialog( USAGE_POPUP, "outpng <filename>" );
		}
		else if( strcmp( tokens[0], "outpnga" ) == 0 )
		{
			pushpop_window( PUSHPOP_ABOVE );
			if ( token_cnt > 1 )
                // TRUE is set for alpha channel
				write_PNG_file( tokens[1], TRUE, png_compression_level );
			else
				popup_dialog( USAGE_POPUP, "outpnga <filename>" );
		}
        else if ( strcmp ( tokens[0], "pngcomp" ) == 0 )
        {
            if ( token_cnt > 1 )
            {
                int compression_level = strtod( tokens[1], (char**)NULL );
                if ( compression_level < 0 || compression_level > 9 || compression_level != floor(compression_level) )
                {
                    // If requested compressiont level not in valid range, don't change
                    wrt_text("\n\n%s\n", "Invalid PNG compression level selected. Valid compression levels are 0-9");
                }
                else
                {
                    png_compression_level = compression_level;
                    wrt_text("\n\n%s %d\n", "PNG compression level set to", png_compression_level);
                }
            }
            else
                popup_dialog( USAGE_POPUP, "pngcomp <compression level>\nAvailable compression levels: 0-9" );
        }
	#endif
	#ifdef JPEG_SUPPORT
		else if ( strcmp( tokens[0], "outjpeg" ) == 0 ||
				  strcmp( tokens[0], "outjpg"  ) == 0)
		{
			pushpop_window( PUSHPOP_ABOVE );
			if ( token_cnt > 1 )
				write_JPEG_file( tokens[1], FALSE, jpeg_quality );
			else
				popup_dialog( USAGE_POPUP, "outjpeg <filename>" );
		}
		else if ( strcmp( tokens[0], "jpegqual" ) == 0 ||
				  strcmp( tokens[0], "jpgqual" ) == 0 )
		{
			if ( token_cnt > 1 )
				jpeg_quality = strtod( tokens[1], (char **)NULL );
			else
				popup_dialog( USAGE_POPUP, "jpegqual <value>" );
		}
	#endif
		/*
			else if ( strcmp( tokens[0], "outvc" ) == 0 )
			{
				if ( token_cnt > 1 )
					write_vv_connectivity_file( tokens[1], analy );
				else
					popup_dialog( USAGE_POPUP, "outvc <filename>" );
			}
			else if ( strcmp( tokens[0], "outvv" ) == 0 )
			{
				if ( token_cnt > 1 )
					write_vv_state_file( tokens[1], analy );
				else
					popup_dialog( USAGE_POPUP, "outvv <filename>" );
			}
		*/
		else if ( strcmp( tokens[0], "pref" ) == 0 )
		{
			sscanf( tokens[1], "%f", &val );
			set_pr_ref( val );
		}
		else if ( strcmp( tokens[0], "dia" ) == 0 )
		{
			sscanf( tokens[1], "%f", &val );
			set_diameter( val );
		}
		else if ( strcmp( tokens[0], "ym" ) == 0 )
		{
			sscanf( tokens[1], "%f", &val );
			set_youngs_mod( val );
		}
		else if ( strcmp( tokens[0], "partrad" ) == 0 )
		{
			sscanf( tokens[1], "%f", &val );
			set_particle_radius( val );
			redraw = BINDING_MESH_VISUAL;
		}
		else if ( strcmp( tokens[0], "outvec" ) == 0 )
		{
			if ( token_cnt == 2 )
				outvec( tokens[1], analy );
			else
				popup_dialog( USAGE_POPUP, "outvec <filename>" );
		}
		else if ( strcmp( tokens[0], "autorgb" ) == 0 )
		{
			if ( token_cnt == 2 )
			{
				str_dup( &analy->img_root, tokens[1] );
				analy->autoimg_format = IMAGE_FORMAT_RGB;
				reset_autoimg_count();
			}
			else
				popup_dialog( USAGE_POPUP,
							  "autorgb <root file name>" );
		}
		else if ( strcmp( tokens[0], "autoimg" ) == 0 )
		{
			if ( token_cnt == 2 )
			{
				str_dup( &analy->img_root, tokens[1] );
				analy->autoimg_format = IMAGE_FORMAT_RGB;
				reset_autoimg_count();
			}
			else if ( token_cnt == 3 && strcmp( tokens[2], "jpeg" ) == 0 )
			{
	#ifdef JPEG_SUPPORT
				Bool_type no_jpeg = FALSE;
	#else
				Bool_type no_jpeg = TRUE;
	#endif

				if ( no_jpeg )
					popup_dialog( INFO_POPUP, "%s\n%s",
								  "JPEG support is not compiled into this "
								  "executable;",
								  "using RGB format." );

				str_dup( &analy->img_root, tokens[1] );
				analy->autoimg_format = no_jpeg
										? IMAGE_FORMAT_RGB : IMAGE_FORMAT_JPEG;
				reset_autoimg_count();
			}
			else if ( token_cnt == 3 && strcmp( tokens[2], "png" ) == 0 )
			{
	#ifdef PNG_SUPPORT
				Bool_type no_png = FALSE;
	#else
				Bool_type no_png = TRUE;
	#endif

				if ( no_png )
					popup_dialog( INFO_POPUP, "%s\n%s",
								  "PNG support is not compiled into this "
								  "executable;",
								  "using RGB format." );

				str_dup( &analy->img_root, tokens[1] );
				analy->autoimg_format = no_png
										? IMAGE_FORMAT_RGB : IMAGE_FORMAT_PNG;
				reset_autoimg_count();
			}
			else
				popup_dialog( USAGE_POPUP,
							  "autoimg <root file name> [jpeg | png]" );
		}
		else if ( strcmp( tokens[0], "resetimg" ) == 0 )
		{
			reset_autoimg_count();
		}
		else if ( strcmp( tokens[0], "numclass" ) == 0 )
		{
			MO_class_data **classarray = NULL;
			int classqty = 0;

			if ( token_cnt > 1 )
				str_dup( &analy->num_class, tokens[0] );
			else
				popup_dialog( USAGE_POPUP,
							  "numclass <nodal or elem class name>..." );

			for ( i = 1; i < token_cnt; i++ )
			{
				rval = htable_search( MESH( analy ).class_table, tokens[i],
									  FIND_ENTRY, &p_hte );
				if ( rval == OK )
				{
					classarray = RENEW_N( MO_class_data *, classarray, classqty, 1,
										  "numbering class pointers" );
					classarray[classqty] = (MO_class_data *) p_hte->data;
					classqty++;
				}
				else if ( strcmp( tokens[i], "all" ) == 0 )
				{
					create_class_list( &classqty, &classarray, MESH_P( analy ), 7,
									   G_NODE, G_TRUSS, G_BEAM, G_TRI, G_QUAD,
									   G_TET, G_HEX );
				}
				else
					popup_dialog( INFO_POPUP,
								  "Invalid class name; command ignored.");
			}

			if ( classarray != NULL )
			{
				if ( analy->classarray != NULL )
					free( analy->classarray );
				analy->classarray = classarray;
				analy->classqty = classqty;
				redraw = BINDING_MESH_VISUAL;
			}
		}
		else if ( strcmp( tokens[0], "outview" ) == 0 )
		{
			/* Hide all other Griz windows */
			pushpop_window( PUSHPOP_BELOW );

			if ( token_cnt == 2 )
			{
				if ( (fp_outview_file = fopen( tokens[1], "w" )) != NULL )
				{
					fprintf( fp_outview_file, "off refresh\n" );
					outview( fp_outview_file, analy );
					fprintf( fp_outview_file, "on refresh\n" );
					fclose( fp_outview_file );
				}
				else
					popup_dialog( WARNING_POPUP,
								  "Unable to open outview file:  %s\n", tokens[1] );
			}
			else
				popup_dialog( INFO_POPUP, "Filename needed for outview command." );
		}
		else if ( strcmp( tokens[0], "initval" ) == 0 )
		{
			if ( token_cnt == 2 )
			{
				val = atof( tokens[1] );
				analy->buffer_init_val = val;
				if ( analy->cur_result != NULL )
				{
					/* redraw = NONBINDING_MESH_VISUAL; */
					analy->result_mod = TRUE;
					redraw = reset_global_minmax( analy );
					if ( redraw != NO_VISUAL_CHANGE )
						load_result( analy, TRUE, TRUE, FALSE );
				}
			}
			else
				popup_dialog( USAGE_POPUP, "initval <value> \n"
							  "(current value = %.2g)", analy->buffer_init_val );
		}
		else if ( strcmp( tokens[0], "surface" ) == 0 )
		{
			if ( TRUE == traction_data_table_initialized )
			{
				/*
				 * "clean up" surface command
				 */

				free( traction_data_table );
			}


			initialize_surface_parameter_table();

			if ( valid_state == parse_surface_command( tokens, token_cnt ) )
			{
				if ( auto_computing_state == surface_parameter_table.n )
				{
					/*
					 * perform autocomputing to establish "n"
					 */

					if ( invalid_state == auto_compute_n( analy ) )
						return;
				}
				else
				{
					if ( NULL != (surface_target = surface_function_lookup( surface_parameter_table.type )) )
					{
						surface_target();

						wrt_text( "\n\n" );
						wrt_text( "Number of points:  %d\n", qty_tdt );
						wrt_text( "surface area:  %f\n", surface_area );
						wrt_text( "p  ==>  %f, %f, %f\n",  surface_parameter_table.p.x,  surface_parameter_table.p.y,  surface_parameter_table.p.z );
						wrt_text( "u0 ==>  %f, %f, %f\n", u0.x, u0.y, u0.z );
						wrt_text( "u1 ==>  %f, %f, %f\n", u1.x, u1.y, u1.z );
						wrt_text( "u2 ==>  %f, %f, %f\n", u2.x, u2.y, u2.z );
					}
					else
						popup_dialog( USAGE_POPUP, "surface:  Invalid surface type" );
				}


				free( surface_parameter_table.type );
			}
			else
				popup_dialog( USAGE_POPUP, "surface {spot | ring | rect | tube | poly }" );
		}
		else if ( strcmp( tokens[0], "traction" ) == 0 )
		{
			if ( TRUE == surface_command_processed )
			{
				if ( valid_state == parse_traction_command( analy, tokens, token_cnt ) )
				{
					traction( analy, traction_material_list );

					free( traction_material_list );
				}
			}
			else
			{
				popup_dialog( WARNING_POPUP, "traction:  A surface command MUST be processed before using traction." );
				traction_data_table_initialized = FALSE;
			}
		}
		/* Null strings don't become tokens. */
		else
		{
			popup_dialog( INFO_POPUP, "Command \"%s\" not valid", tokens[0] );
			valid_command = FALSE;
		}
    }

    if ( renorm )
        compute_normals( analy );

    if ( redraw!=BINDING_PLOT_VISUAL )
        analy->th_plotting = FALSE;

    switch ( redraw )
    {
    case BINDING_MESH_VISUAL:
        if ( analy->render_mode != RENDER_MESH_VIEW )
            manage_render_mode( RENDER_MESH_VIEW, analy, TRUE );
        /* Note no break, want to fall into next case. */

    case NONBINDING_MESH_VISUAL:
        if ( analy->render_mode == RENDER_MESH_VIEW )
        {
            if ( analy->center_view )
                center_view( analy );
            if ( analy->refresh )
                analy->update_display( analy );
        }
        break;

    case BINDING_PLOT_VISUAL:
        if ( analy->render_mode != RENDER_PLOT )
            manage_render_mode( RENDER_PLOT, analy, TRUE );
        /* Note no break, want to fall into next case. */

    case NONBINDING_PLOT_VISUAL:
        if ( analy->refresh && analy->render_mode == RENDER_PLOT )
        {
            analy->update_display( analy );
        }
        break;

    default:
        /* case NO_VISUAL_CHANGE - do nothing. */
        ;
    }

    analy->result_mod = FALSE;

    if(valid_command)
    {
        history_command(buf);
    }
    else
    {
        if(env.history_input_active == FALSE)
        {
            fclose(analy->p_histfile);
            i = truncate(analy->hist_fname, pos);
            analy->p_histfile = fopen(analy->hist_fname, "at");
        }
    }

    if ( strcmp( tokens[0], "r" ) != 0 )
        if(strlen(buf) < LASTCMD)
        {
            strcpy( last_command, buf );
        }
}

/****************************************************************
 * TAG( include_all_elements )
 * turns on visibility for all elements in addition to materials
 *
 *
 */
int include_all_elements(Analysis *analy)
{
    MO_class_data **class_array, *p_mocd;
    Mesh_data *p_md;
    int class_qty = 0;
    int qty_selected = 0;
    int i, j, qty_elements, status = OK;
    p_md = MESH_P(analy);
    status = htable_get_data(p_md->class_table,
                             (void ***) &class_array,
                             &class_qty);
    qty_selected = MESH(analy).qty_class_selections;
    for(i = 0; i < qty_selected; i++)
    {
        p_mocd = MESH( analy ).by_class_select[i].p_class;
        if(p_mocd == NULL)
            continue;
        if(!is_elem_class(p_mocd->superclass))
            continue;
        qty_elements = p_mocd->qty;
        for(j = 0; j < qty_elements; j++)
        {
            MESH(analy).by_class_select[i].hide_class_elem[j] = FALSE;
            MESH(analy).by_class_select[i].disable_class_elem[j] = FALSE;
        }

        MESH(analy).by_class_select[i].hide_class_elem_qty = 0;
        MESH(analy).by_class_select[i].disable_class_elem_qty = 0;

    }

    return 0;
}

/*****************************************************************
 * TAG( redraw_for_render_mode )
 *
 * Return a redraw mode consistent with current render mode and
 * binding flag.  If parameter "exclusive" is not RENDER_ANY,
 * the current render mode must equal the value of "exclusive" or
 * NO_VISUAL_CHANGE is returned.
 */
extern Redraw_mode_type
redraw_for_render_mode( Bool_type binding, Render_mode_type exclusive,
                        Analysis *analy )
{
    switch ( analy->render_mode )
    {
    case RENDER_MESH_VIEW:
        if ( exclusive == RENDER_ANY
                || exclusive == RENDER_MESH_VIEW )
        {
            if ( binding )
                return BINDING_MESH_VISUAL;
            else
                return NONBINDING_MESH_VISUAL;
        }
        else
            return NO_VISUAL_CHANGE;

    case RENDER_PLOT:
        if ( exclusive == RENDER_ANY
                || exclusive == RENDER_PLOT )
        {
            if ( binding )
                return BINDING_PLOT_VISUAL;
            else
                return NONBINDING_PLOT_VISUAL;
        }
        else
            return NO_VISUAL_CHANGE;

    default:
        popup_dialog( WARNING_POPUP,
                      "Un-implemented render mode in set redraw." );
    }

    return NO_VISUAL_CHANGE;
}

/*****************************************************************
 * TAG( manage_render_mode )
 *
 * Take care of transitions between different render modes.
 */
void
manage_render_mode( Render_mode_type new_rmode, Analysis *analy,
                    Bool_type suppress )
{
    /* Yes, a hack, but we can remove it when we add multiple windows. */
    if ( suppress )
        suppress_display_updates( TRUE );

    switch ( new_rmode )
    {
    case RENDER_MESH_VIEW:
        if ( analy->render_mode != RENDER_MESH_VIEW )
        {
            analy->update_display = draw_mesh_view;
            analy->display_mode_refresh = update_vis;
            analy->check_mod_required = mod_required_mesh_mode;
#ifndef SERIAL_BATCH
            update_gui( analy, new_rmode, analy->render_mode );
#endif
            analy->render_mode = RENDER_MESH_VIEW;
        }
        break;

    case RENDER_PLOT:
        if ( analy->render_mode != RENDER_PLOT )
        {
            analy->update_display = draw_plots;
            analy->display_mode_refresh = update_plots;
            analy->check_mod_required = mod_required_plot_mode;
#ifndef SERIAL_BATCH
            update_gui( analy, new_rmode, analy->render_mode );
#endif
            analy->render_mode = RENDER_PLOT;
        }
        break;

    default:
        popup_dialog( WARNING_POPUP,
                      "Un-implemented render mode in render mode update." );
    }

    if ( suppress )
        suppress_display_updates( FALSE );
}

/*****************************************************************
 * TAG( parse_mtl_cmd )
 *
 * Parse a material manager command.
 */
static void
parse_mtl_cmd( Analysis *analy, char tokens[][TOKENLENGTH], int token_cnt, Bool_type src_mtl_mgr, Redraw_mode_type *p_redraw, Bool_type *p_renorm )
{
    int i, first_token, cnt, read, mtl_qty, qty_vals;
    static Bool_type preview_set = FALSE;
    static Bool_type incomplete_cmd = FALSE;
    Bool_type parsing;
    Redraw_mode_type redraw;
    static GLfloat *save_props[MTL_PROP_QTY];
    GLfloat *p_src, *p_dest, *bound;
    Material_property_type j;

    if ( token_cnt == 1 )
        return;

    mtl_qty = analy->max_mesh_mat_qty;

    if ( src_mtl_mgr )
        first_token = 1;
    else
        first_token = 0;

    i = first_token;
    cnt = token_cnt - first_token;
    parsing = TRUE;
    while( parsing )
    {
        if ( incomplete_cmd == TRUE )
        {
            read = parse_embedded_mtl_cmd( analy, tokens + i, cnt,
                                           preview_set, save_props,
                                           p_renorm );
            incomplete_cmd = FALSE;
            if ( read + first_token >= token_cnt )
            {
                parsing = FALSE;
                redraw = BINDING_MESH_VISUAL;
            }
            else
                i += read;
        }
        else if ( strcmp( tokens[i], "preview" ) == 0 )
        {
        	//TO DO
            i++;
            preview_set = TRUE;
        }
        else if ( strcmp( tokens[i], "apply" ) == 0 )
        {
            /* Free saved copies of material properties. */
            for ( j = AMBIENT; j < MTL_PROP_QTY; j++ )
            {
                if ( save_props[j] != NULL )
                {
                    free( save_props[j] );
                    save_props[j] = NULL;
                }
            }
            preview_set = FALSE;
            parsing = FALSE;
            redraw = NO_VISUAL_CHANGE;
        }
        else if ( strcmp( tokens[i], "cancel" ) == 0 )
        {
            /* Replace current structures with saved copies. */
            for ( j = AMBIENT; j < MTL_PROP_QTY; j++ )
            {
                if ( save_props[j] != NULL )
                {
                    if ( j == AMBIENT )
                        p_dest = &v_win->mesh_materials.ambient[0][0];
                    else if ( j == DIFFUSE )
                        p_dest = &v_win->mesh_materials.diffuse[0][0];
                    else if ( j == SPECULAR )
                        p_dest = &v_win->mesh_materials.specular[0][0];
                    else if ( j == EMISSIVE )
                        p_dest = &v_win->mesh_materials.emission[0][0];
                    else if ( j == SHININESS )
                        p_dest = &v_win->mesh_materials.shininess[0];

                    qty_vals = ( j == SHININESS ) ? mtl_qty : 4 * mtl_qty;
                    bound = &save_props[j][0] + qty_vals;
                    p_src = &save_props[j][0];

                    for ( ; p_src < bound; *p_dest++ = *p_src++ );
                    free( save_props[j] );
                    save_props[j] = NULL;
                }
            }
            preview_set = FALSE;
            parsing = FALSE;
            redraw = BINDING_MESH_VISUAL;
        }
        else if ( strcmp( tokens[i],  "continue" ) == 0 )
        {
            parsing = FALSE;
            incomplete_cmd = TRUE;
            redraw = NO_VISUAL_CHANGE;
        }
        else
        {
            read = parse_embedded_mtl_cmd( analy, tokens + i, cnt,
                                           preview_set, save_props,
                                           p_renorm );
            if ( read + first_token >= token_cnt )
            {
                parsing = FALSE;
                redraw = BINDING_MESH_VISUAL;
            }
            else
                i += read;
        }
    }

    *p_redraw = redraw;
}

/*****************************************************************
 * TAG( parse_embedded_mtl_cmd )
 *
 * Parse a subdivision of a material manager command.  This may
 * be a typical "mat", "invis", etc. command, or a (new)
 * combination command ("invis disable...") or one with multiple
 * materials as operands.
 *
 * This command is stateful so that multiple calls may be used
 * to complete one command.
 */
static int
parse_embedded_mtl_cmd( Analysis *analy, char tokens[][TOKENLENGTH], int cnt,
                        Bool_type preview, GLfloat *save[MTL_PROP_QTY],
                        Bool_type *p_renorm )
{
    static Bool_type visible = FALSE;
    static Bool_type invisible = FALSE;
    static Bool_type enable = FALSE;
    static Bool_type disable = FALSE;
    static Bool_type mat = FALSE;
    static Bool_type modify_all = FALSE;
    static Bool_type apply_defaults = FALSE;
    static Bool_type reading_materials = FALSE;
    static Bool_type reading_cprop[MTL_PROP_QTY];
    static Material_property_obj props;
    static int current_mtl_qty;
    int i, limit, mtl, mqty, k, l;
    Material_property_type j;
    unsigned char *disable_mtl, *hide_mtl;

    int mat_min, mat_max;

    i = 0;

    /*
     * Finish up any partial segment.
     */
    if ( reading_materials )
    {
        /* Read material numbers until end of list or a word is encountered. */
        while ( i < cnt && (is_numeric_token( tokens[i] ) ||
                            is_numeric_range_token( tokens[i] )) )
        {
            /* IRC - June 04, 2004: Added material range parsing */
            if ( is_numeric_range_token( tokens[i] ) )
            {
                parse_mtl_range(tokens[i], MAX_MATERIALS, &mat_min, &mat_max);

                if(mat_min < 0 || mat_min > MAX_MATERIALS || mat_max < 0 || mat_max > MAX_MATERIALS ||
                   mat_min > mat_max)
                {
                    popup_dialog(INFO_POPUP, "specified material numbers out of range.\n");
                    return 0;
                }

                mtl = mat_min ;
                for (mtl =  mat_min ;
                        mtl <= mat_max ;
                        mtl++)
                {
                    props.materials[props.mtl_cnt++] = mtl ;
                }
            }
            else
            {
                mtl = (atoi ( tokens[i] ) - 1);
                props.materials[props.mtl_cnt++] = mtl;
            }
            i++;
        }

        /*
         * If we ended reading materials for any reason other than
         * encountering the "continue" token, we're done reading materials.
         * We could be done even if "continue" occurs, but that can't be
         * known until the next call.
         */
        if ( i == cnt || strcmp( tokens[i], "continue" ) != 0 )
            reading_materials = FALSE;
        else
            return i;
    }
    else if ( mat && props.cur_idx != 0 )
    {
        /*
         * Must be parsing a color property.  This assumes that when the
         * "mat" string is parsed there will always be at least one material
         * number to parse IN THE SAME CALL.  This is a safe assumption as
         * "mat" will be at most the third token and MAX_TOKENS should never
         * need to be set small (i.e., less than four), therefore we will
         * have at least started reading material numbers before encountering
         * the end of the tokens.  To have dropped into this block, reading
         * materials will have been finished, so if anything remains to be
         * read it must be color properties.
         */
        for ( j = AMBIENT; j < MTL_PROP_QTY; j++ )
            if ( reading_cprop[j] )
                break;

        if ( j == SHININESS )
            props.color_props[0] = atof( tokens[0] );
        else if ( j != MTL_PROP_QTY )
        {
            limit = 3 - props.cur_idx;
            for ( ; i < limit; i++ )
                props.color_props[props.cur_idx++] =
                    (GLfloat) atof( tokens[i] );
            props.cur_idx = 0;
        }
    }

    /* Parse (rest of) tokens. */
    for ( ; i < cnt; i++ )
    {
        if ( is_numeric_token( tokens[i] ) ||
                is_numeric_range_token( tokens[i] ))
        {
            /* Material numbers precede any other numeric tokens. */
            if ( props.mtl_cnt == 0 )
            {
                reading_materials = TRUE;

                if ( current_mtl_qty != analy->max_mesh_mat_qty
                        && current_mtl_qty != 0 )
                {
                    free( props.materials );
                    props.materials = NULL;
                    current_mtl_qty = 0;
                }

                if ( props.materials == NULL )
                {
                    props.materials = NEW_N( int, analy->max_mesh_mat_qty,
                                             "Properties material array" );
                    current_mtl_qty = analy->max_mesh_mat_qty;
                }
            }

            if ( reading_materials )
            {
                /* mtl = (atoi ( tokens[i] ) - 1) % MAX_MATERIALS; */

                if ( is_numeric_range_token( tokens[i] ) )
                {
                    parse_mtl_range(tokens[i], MAX_MATERIALS, &mat_min, &mat_max );

                    if(mat_min < 0 || mat_min > MAX_MATERIALS || mat_max < 0 || mat_max > MAX_MATERIALS ||
                         mat_min > mat_max)
                    {
                       popup_dialog(INFO_POPUP, "specified material numbers out of range.\n");
                       return 0;
                    }

                    mtl = mat_min ;
                    for (mtl =  mat_min ;
                            mtl <= mat_max ;
                            mtl++)
                    {
                        props.materials[props.mtl_cnt++] = mtl ;
                    }
                }
                else
                {
                    mtl = (atoi ( tokens[i] ) - 1);
                    props.materials[props.mtl_cnt++] = mtl;
                }
            }
            else if ( mat )
            {
                props.color_props[props.cur_idx++] =
                    (GLfloat) atof( tokens[i] );
            }
            else
                popup_dialog( WARNING_POPUP, "Bad Mtl Mgr numeric token parse." );
        }
        else if ( strcmp( tokens[i], "continue" ) == 0 )
            /* Kick out before incrementing i again. */
            break;
        else if ( mat )
        {
            /* Must be the start of a color property specification. */
            reading_materials = FALSE;
            /* mqty = ( analy->num_materials < MAX_MATERIALS )
                   ? analy->num_materials : MAX_MATERIALS; */
            mqty = analy->max_mesh_mat_qty;
            if ( strcmp( tokens[i], "amb" ) == 0 )
            {
                if ( preview )
                    copy_property( AMBIENT, &v_win->mesh_materials.ambient[0][0],
                                   &save[AMBIENT], mqty );
                update_previous_prop( AMBIENT, reading_cprop, &props );
                reading_cprop[AMBIENT] = TRUE;
            }
            else if ( strcmp( tokens[i], "diff" ) == 0 )
            {
                if ( preview )
                    copy_property( DIFFUSE, &v_win->mesh_materials.diffuse[0][0],
                                   &save[DIFFUSE], mqty );
                update_previous_prop( DIFFUSE, reading_cprop, &props );
                reading_cprop[DIFFUSE] = TRUE;
            }
            else if ( strcmp( tokens[i], "spec" ) == 0 )
            {
                if ( preview )
                    copy_property( SPECULAR, &v_win->mesh_materials.specular[0][0],
                                   &save[SPECULAR], mqty );
                update_previous_prop( SPECULAR, reading_cprop, &props );
                reading_cprop[SPECULAR] = TRUE;
            }
            else if ( strcmp( tokens[i], "emis" ) == 0 )
            {
                if ( preview )
                    copy_property( EMISSIVE, &v_win->mesh_materials.emission[0][0],
                                   &save[EMISSIVE], mqty );
                update_previous_prop( EMISSIVE, reading_cprop, &props );
                reading_cprop[EMISSIVE] = TRUE;
            }
            else if ( strcmp( tokens[i], "shine" ) == 0 )
            {
                if ( preview )
                    copy_property( SHININESS, &v_win->mesh_materials.shininess[0],
                                   &save[SHININESS], mqty );
                update_previous_prop( SHININESS, reading_cprop, &props );
                reading_cprop[SHININESS] = TRUE;
            }
            else if ( strcmp( tokens[i], "gslevel" ) == 0 ||
                      strcmp( tokens[i], "gs" ) == 0 )
            {
                i++;
                for ( k = 0;
                        k < props.mtl_cnt;
                        k++ )
                {
                    mtl = props.materials[k];
                    v_win->mesh_materials.gslevel[mtl] = (GLfloat) atof( tokens[i] );
                }
            }
            else
                popup_dialog( WARNING_POPUP, "Bad Mtl Mgr parse. Color Property Must = [amb | diff | spec || emis | shine]" );

        }
        else if ( strcmp( tokens[i], "vis" ) == 0 )
            visible = TRUE;
        else if ( strcmp( tokens[i], "invis" ) == 0 )
            invisible = TRUE;
        else if ( strcmp( tokens[i], "enable" ) == 0 )
            enable = TRUE;
        else if ( strcmp( tokens[i], "disable" ) == 0 )
            disable = TRUE;
        else if ( strcmp( tokens[i], "mat" ) == 0 )
            mat = TRUE;
        else if ( strcmp( tokens[i], "default" ) == 0 )
            apply_defaults = TRUE;
        else if ( strcmp( tokens[i], "all" ) == 0 )
        {
            if ( invisible || visible || enable || disable )
                modify_all = TRUE;
        }
        else
            popup_dialog( WARNING_POPUP, "Bad Mtl Mgr parse." );
    }

    if ( i >= cnt )
    {
        /*
         * Didn't encounter "continue", so clean-up and reset
         * static variables for next new command.
         */
        if ( mat )
        {
            update_previous_prop( MTL_PROP_QTY, reading_cprop, &props );
            mat = FALSE;
        }
        else if ( apply_defaults )
        {
            define_color_properties( &v_win->mesh_materials,
                                     props.materials, props.mtl_cnt,
                                     material_colors, MATERIAL_COLOR_CNT );

            apply_defaults = FALSE;
        }
        else
        {
            mqty = MESH( analy ).material_qty;

            if ( visible || invisible )
            {
                hide_mtl = MESH( analy ).hide_material;

                if ( modify_all )
                {
                    for ( k = 0; k < mqty; k++ )
                        hide_mtl[k] = (unsigned char) invisible;
                    modify_all = FALSE;
                }
                else
                    for ( k = 0; k < props.mtl_cnt; k++ )
                        hide_mtl[props.materials[k]] = (unsigned char) invisible;

                reset_face_visibility( analy );
                if ( analy->dimension == 3 ) *p_renorm = TRUE;
                analy->result_mod = TRUE;
                visible = FALSE;
                invisible = FALSE;
            }

            if ( enable || disable )
            {
                disable_mtl = MESH( analy ).disable_material;

                if ( modify_all )
                {
                    for ( k = 0; k < mqty; k++ )
                        disable_mtl[k] = (unsigned char) disable;
                    modify_all = FALSE;
                }
                else
                    for ( k = 0; k < props.mtl_cnt; k++ )
                        disable_mtl[props.materials[k]] = (unsigned char) disable;

                analy->result_mod = TRUE;
                load_result( analy, TRUE, TRUE, FALSE );
                enable = FALSE;
                disable = FALSE;
            }
        }
        reading_materials = FALSE;
        props.mtl_cnt = 0;
    }

    return i;
}

/*****************************************************************
 * TAG( parse_mtl_range)
 *
 * This function will parse a a material range tolken and return
 * the min material and max material. This function will also
 * parse a single value token.
 */
void
parse_mtl_range( char *token, int qty, int *mat_min, int *mat_max )
{
    int i=0;
    char temp_token[64];

    Bool_type range_found=FALSE;


    *mat_min = 0;
    *mat_max = 0;

    string_to_upper( token, temp_token ); /* Make case insensitive */

    /* First check for all materials */

    if ( strcmp(temp_token , "ALL" ) == 0 )
    {
        *mat_min = 1;
        *mat_max = qty;
        return;
    }


    for ( i = 1;
            i < strlen(temp_token);
            i++ )
    {
        if ( temp_token[i] == '-' || temp_token[i] == ':' )
        {
            range_found=TRUE;
            temp_token[i] = ' ';
        }
    }

    if ( range_found )
        sscanf(temp_token, "%d %d", mat_min, mat_max);
    else
    {
        /* Parse a single token value */
        sscanf(temp_token, "%d", mat_min);
        *mat_max = *mat_min;
    }

}

/*****************************************************************
 * TAG( parse_embedded_surf_cmd )
 *
 * Parse a subdivision of a surface manager command.  This may
 * be a typical "vis", "invis", etc. command, or a (new)
 * combination command ("invis disable...") or one with multiple
 * surfaces as operands.
 *
 * This command is stateful so that multiple calls may be used
 * to complete one command.
 */
static int
parse_embedded_surf_cmd( Analysis *analy, char tokens[][TOKENLENGTH], int cnt,
                         Bool_type *p_renorm )
{
    static Bool_type visible = FALSE;
    static Bool_type invisible = FALSE;
    static Bool_type enable = FALSE;
    static Bool_type disable = FALSE;
    static Bool_type modify_all = FALSE;
    static Bool_type apply_defaults = FALSE;
    static Bool_type reading_surfaces = FALSE;
    static Surface_property_obj props;
    static int current_surf_qty;
    int i, limit, surf, sqty, k;
    unsigned char *disable_surf, *hide_surf;

    i = 0;

    /*
     * Finish up any partial segment.
     */
    if ( reading_surfaces )
    {
        /* Read surface numbers until end of list or a word is encountered. */
        while ( i < cnt && is_numeric_token( tokens[i] ) )
        {
            surf = (atoi ( tokens[i] ) - 1);
            props.surfaces[props.surf_cnt++] = surf;
            i++;
        }

        /*
         * If we ended reading surfaces for any reason other than
         * encountering the "continue" token, we're done reading surfaces.
         * We could be done even if "continue" occurs, but that can't be
         * known until the next call.
         */
        if ( i == cnt || strcmp( tokens[i], "continue" ) != 0 )
            reading_surfaces = FALSE;
        else
            return i;
    }

    /* Parse (rest of) tokens. */
    for ( ; i < cnt; i++ )
    {
        if ( is_numeric_token( tokens[i] ) )
        {
            /* Material numbers precede any other numeric tokens. */
            if ( props.surf_cnt == 0 )
            {
                reading_surfaces = TRUE;

                if ( current_surf_qty != analy->max_mesh_surf_qty
                        && current_surf_qty != 0 )
                {
                    free( props.surfaces );
                    props.surfaces = NULL;
                    current_surf_qty = 0;
                }

                if ( props.surfaces == NULL )
                {
                    props.surfaces = NEW_N( int, analy->max_mesh_surf_qty,
                                            "Properties surface array" );
                    current_surf_qty = analy->max_mesh_surf_qty;
                }
            }

            /* Surface numbers precede any other numeric tokens. */
            if ( reading_surfaces )
            {
                surf = (atoi ( tokens[i] ) - 1);
                props.surfaces[props.surf_cnt++] = surf;
            }
            else
                popup_dialog( WARNING_POPUP, "Bad Surface Mgr numeric token parse." );
        }
        else if ( strcmp( tokens[i], "continue" ) == 0 )
            /* Kick out before incrementing i again. */
            break;
        else if ( strcmp( tokens[i], "vis" ) == 0 )
            visible = TRUE;
        else if ( strcmp( tokens[i], "invis" ) == 0 )
            invisible = TRUE;
        else if ( strcmp( tokens[i], "enable" ) == 0 )
            enable = TRUE;
        else if ( strcmp( tokens[i], "disable" ) == 0 )
            disable = TRUE;
        else if ( strcmp( tokens[i], "default" ) == 0 )
            apply_defaults = TRUE;
        else if ( strcmp( tokens[i], "all" ) == 0 )
        {
            if ( invisible || visible || enable || disable )
                modify_all = TRUE;
        }
        else
            popup_dialog( WARNING_POPUP, "Bad Surface Mgr parse." );
    }

    if ( i >= cnt )
    {
        /*
         * Didn't encounter "continue", so clean-up and reset
         * static variables for next new command.
         */
        sqty = MESH( analy ).surface_qty;

        if ( visible || invisible )
        {
            hide_surf = MESH( analy ).hide_surface;

            if ( modify_all )
            {
                for ( k = 0; k < sqty; k++ )
                    hide_surf[k] = (unsigned char) invisible;
                modify_all = FALSE;
            }
            else
                for ( k = 0; k < props.surf_cnt; k++ )
                    hide_surf[props.surfaces[k]] = (unsigned char) invisible;

            reset_face_visibility( analy );
            if ( analy->dimension == 3 ) *p_renorm = TRUE;
            analy->result_mod = TRUE;
            visible = FALSE;
            invisible = FALSE;
        }

        if ( enable || disable )
        {
            disable_surf = MESH( analy ).disable_surface;

            if ( modify_all )
            {
                for ( k = 0; k < sqty; k++ )
                    disable_surf[k] = (unsigned char) disable;
                modify_all = FALSE;
            }
            else
                for ( k = 0; k < props.surf_cnt; k++ )
                    disable_surf[props.surfaces[k]] = (unsigned char) disable;

            analy->result_mod = TRUE;
            load_result( analy, TRUE, TRUE, FALSE );
            enable = FALSE;
            disable = FALSE;
        }
        reading_surfaces = FALSE;
        props.surf_cnt = 0;
    }

    return i;
}

/*****************************************************************
 * TAG( update_previous_prop )
 *
 * Check for a property specified for update and do so if found.
 */
static void
update_previous_prop( Material_property_type cur_property,
                      Bool_type update[MTL_PROP_QTY],
                      Material_property_obj *props )
{

    /*** Verify use of v_win->mesh_materials.XXX  LAS ***/


    Material_property_type j;
    int mtl, k;
    GLfloat (*p_dest)[4];

    for ( j = AMBIENT; j < MTL_PROP_QTY; j++ )
        if ( j != cur_property && update[j] )
            break;
    if ( j == MTL_PROP_QTY )
        return;

    /* Get pointer to appropriate array for update. */
    if ( j == AMBIENT )
        p_dest = v_win->mesh_materials.ambient;
    else if ( j == DIFFUSE )
        p_dest = v_win->mesh_materials.diffuse;
    else if ( j == SPECULAR )
        p_dest = v_win->mesh_materials.specular;
    else if ( j == EMISSIVE )
        p_dest = v_win->mesh_materials.emission;

    /* Update material property array. */
    for ( k = 0; k < props->mtl_cnt; k++ )
    {
        mtl = props->materials[k];

        if ( j == SHININESS )
            v_win->mesh_materials.shininess[mtl] = props->color_props[0];
        else
        {
            p_dest[mtl][0] = props->color_props[0];
            p_dest[mtl][1] = props->color_props[1];
            p_dest[mtl][2] = props->color_props[2];
            p_dest[mtl][3] = 1.0;
        }

        /*** ORIGINAL:

        if ( mtl == v_win->current_material )
            update_current_material( j );

        REPLACEMENT:  LAS ***/

        if ( mtl == v_win->mesh_materials.current_index )
            update_current_color_property( &v_win->mesh_materials, j );
    }

    /* Clean-up for another specification. */
    update[j] = FALSE;
    props->cur_idx = 0;
    props->color_props[0] = 0.0;
    props->color_props[1] = 0.0;
    props->color_props[2] = 0.0;
    props->gslevel        = 0.0;
}

/*****************************************************************
 * TAG( copy_property )
 *
 * Copy a material property array.
 */
static void
copy_property( Material_property_type property, GLfloat *source,
               GLfloat **dest, int mqty )
{
    GLfloat *p_dest, *p_src, *bound;
    int qty_vals;

    if ( property == SHININESS )
        qty_vals = mqty;
    else
        qty_vals = mqty * 4;

    *dest = NEW_N( GLfloat, qty_vals, "Mtl prop copy" );
    p_dest = *dest;
    bound = source + qty_vals;
    for ( p_src = source; p_src < bound; *p_dest++ = *p_src++ );
}

/*****************************************************************
 * TAG( parse_surf_cmd )
 *
 * Parse a surface manager command.
 */
static void
parse_surf_cmd( Analysis *analy, char tokens[][TOKENLENGTH],
                int token_cnt, Bool_type src_surf_mgr, Redraw_mode_type *p_redraw,
                Bool_type *p_renorm )
{
    int i, first_token, cnt, read, surf_qty, qty_vals;
    static Bool_type incomplete_cmd = FALSE;
    Bool_type parsing;
    Redraw_mode_type redraw;
    GLfloat *p_src, *p_dest, *bound;

    if ( token_cnt == 1 )
        return;

    surf_qty = analy->max_mesh_surf_qty;

    if ( src_surf_mgr )
        first_token = 1;
    else
        first_token = 0;

    i = first_token;
    cnt = token_cnt - first_token;
    parsing = TRUE;
    while( parsing )
    {
        if ( incomplete_cmd == TRUE )
        {
            read = parse_embedded_surf_cmd( analy, tokens + i, cnt,
                                            p_renorm );
            incomplete_cmd = FALSE;
            if ( read + first_token >= token_cnt )
            {
                parsing = FALSE;
                redraw = BINDING_MESH_VISUAL;
            }
            else
                i += read;
        }
        else if ( strcmp( tokens[i], "apply" ) == 0 )
        {
            parsing = FALSE;
            redraw = NO_VISUAL_CHANGE;
        }
        else if ( strcmp( tokens[i], "cancel" ) == 0 )
        {
            parsing = FALSE;
            redraw = BINDING_MESH_VISUAL;
        }
        else if ( strcmp( tokens[i],  "continue" ) == 0 )
        {
            parsing = FALSE;
            incomplete_cmd = TRUE;
            redraw = NO_VISUAL_CHANGE;
        }
        else
        {
            read = parse_embedded_surf_cmd( analy, tokens + i, cnt,
                                            p_renorm );
            if ( read + first_token >= token_cnt )
            {
                parsing = FALSE;
                redraw = BINDING_MESH_VISUAL;
            }
            else
                i += read;
        }
    }

    *p_redraw = redraw;
}

/*****************************************************************
 * TAG( parse_vcent )
 *
 * Parse "vcent" (view centering) command.
 *
 * Note: assumes a node belongs to the required node class.
 */
static void
parse_vcent( Analysis *analy, char tokens[MAXTOKENS][TOKENLENGTH],
             int token_cnt, Redraw_mode_type *p_redraw )
{
    int ival;
    if ( token_cnt > 1 && strcmp( tokens[1], "off" ) == 0 )
    {
        analy->center_view = NO_CENTER;
        *p_redraw = NO_VISUAL_CHANGE;
    }
    else if ( token_cnt > 1 && strcmp( tokens[1], "hi" ) == 0 )
    {
        analy->center_view = HILITE;
        center_view( analy );
        *p_redraw = BINDING_MESH_VISUAL;
    }
    else if ( token_cnt > 1 && strcmp( tokens[1], "sel" ) == 0){
		analy->center_view = SELECT;
        center_view( analy );
        *p_redraw = BINDING_MESH_VISUAL;
	}
    else if ( token_cnt > 1
              && ( strcmp( tokens[1], "n" ) == 0
                   || strcmp( tokens[1], "node" ) == 0 ) )
    {
        ival = atoi( tokens[2] );
        ival = get_class_label_index(MESH(analy).node_geom, ival);
        if( ival == M_INVALID_LABEL)
            popup_dialog( INFO_POPUP,
                          "Invalid node specified for view center");
        else
        {
            analy->center_view = NODE;
            analy->center_node = ival;
            center_view( analy );
            *p_redraw = BINDING_MESH_VISUAL;
        }
    }
    else if ( token_cnt == 4 )
    {
        analy->view_center[0] = atof( tokens[1] );
        analy->view_center[1] = atof( tokens[2] );
        analy->view_center[2] = atof( tokens[3] );
        analy->center_view = POINT;
        center_view( analy );
        *p_redraw = BINDING_MESH_VISUAL;
    }
    else
        popup_dialog( USAGE_POPUP, "vcent off\n%s\n%s\n%s\n%s",
                      "vcent hi",
                      "vcent sel",
                      "vcent n|node <node_number>",
                      "vcent <x> <y> <z>" );
}

/*****************************************************************
 * TAG( check_visualizing )
 *
 * Test whether cutplanes, isosurfaces, or result vectors are
 * being displayed.  If all of these options are off, reset the
 * display to show the mesh surface.  Otherwise, set up to display
 * just the mesh edges.
 */
static void
check_visualizing( Analysis *analy )
{
    /*
     * This logic needs to save the render_mode when transitioning
     * to a "show_cut", "show_isosurfs",... mode so that the
     * correct render_mode can be reinstated when transitioning
     * back.  Someday...
     */

    if ( analy->show_cut || analy->show_isosurfs ||
            analy->show_vectors )
        /*         analy->show_vectors || analy->show_carpet ) */
    {
        analy->show_edges_vec = TRUE;
        analy->mesh_view_mode = RENDER_NONE;
    }
    else
    {
        analy->show_edges_vec = FALSE;
        analy->mesh_view_mode = RENDER_FILLED;
        update_util_panel( VIEW_SOLID, NULL );
    }

    update_util_panel( VIEW_EDGES, NULL );
}

/*****************************************************************
 * TAG( update_vis )
 *
 * Update the fine cutting plane, contours, and isosurfaces.
 * This routine is called when the state changes and when
 * the displayed result changes.
 */
void
update_vis( Analysis *analy )
{
#ifdef SERIAL_BATCH
#else
    set_alt_cursor( CURSOR_WATCH );
#endif

    gen_cuts( analy );
    gen_isosurfs( analy );
    update_vec_points( analy );
    gen_contours( analy );

#ifdef SERIAL_BATCH
#else
    unset_alt_cursor();
#endif
}

/*****************************************************************
 * TAG( create_alias )
 *
 * Create a command alias.
 */
static void
create_alias( int token_cnt, char tokens[MAXTOKENS][TOKENLENGTH] )
{
    Alias_obj *alias;
    int i;

    if ( token_cnt <3 )
        return;

    /* See if this alias is already being used. */
    for ( alias = alias_list; alias != NULL; alias = alias->next )
    {
        if ( strcmp( tokens[1], alias->alias_str ) == 0 )
        {
            for ( i = 2;
                    i < token_cnt;
                    i++ )
            {
                strcpy( alias->tokens[i-2], tokens[i] );
            }
            alias->token_cnt = token_cnt - 2;
            return;
        }
    }

    alias = NEW( Alias_obj, "Command alias" );

    strcpy( alias->alias_str, tokens[1] );

    for ( i = 2; i < token_cnt; i++ )
    {
        strcpy( alias->tokens[i-2], tokens[i] );
    }
    alias->token_cnt = token_cnt - 2;

    INSERT( alias, alias_list );
}

/*****************************************************************
 * TAG( delete_alias )
 *
 * Delete a command alias.
 */
static void
delete_alias( int token_cnt, char tokens[MAXTOKENS][TOKENLENGTH] )
{
    Alias_obj *alias;
    int i;

    if ( token_cnt < 2 )
        return;

    printf("\nToken=%s-%s\n\n", tokens[0], tokens[1]);

    /* See if this alias is already being used. */
    for ( alias = alias_list; alias != NULL; alias = alias->next )
    {
        if ( strcmp( tokens[1], alias->alias_str ) == 0 )
        {
            /* Found the matching alias - now remove it */

            DELETE( alias, alias_list );
            return;
        }
    }
    return;
}

/*****************************************************************
 * TAG( list_alias )
 *
 * Print a list of all command alias.
 */
static void
list_alias( )
{
    Alias_obj *alias;
    char alias_names[MAXTOKENS*TOKENLENGTH];
    int alias_count=0;
    int i,j;

    /* See if this alias is already being used. */
    alias = alias_list;
    alias_names[0]='\0';
    strcat(alias_names, " Alias List:" );
    for ( alias = alias_list; alias != NULL; alias = alias->next )
    {
        strcat(alias_names, "\n  " );
        strcat(alias_names, alias->alias_str );
        strcat( alias_names, "  \"" );
        for ( j = 0;
                j < alias->token_cnt;
                j++ )
        {
            strcat( alias_names, alias->tokens[j] );
            if (j<alias->token_cnt-1)
                strcat( alias_names, " " );
        }
        strcat( alias_names, "\"" );
        alias_count++;
    }
    if ( alias_count>0 )
        popup_dialog( INFO_POPUP,
                      alias_names );
    return;
}

/*****************************************************************
 * TAG( alias_expand )
 *
 *  Expand multi-line alias to single string.
 */
static void
alias_expand( char *buf, int *token_cnt )
{
    char tokens[MAXTOKENS][TOKENLENGTH];
    char tmp_buf[2000];
    int i=0, j=0;

    strcpy( tmp_buf, buf );
    for ( i=0;
            i<strlen(buf);
            i++ )
    {
        if ( buf[i]==';' )
        {
            tmp_buf[j++] = ' ';
            tmp_buf[j++] = buf[i]; /* Make ; a single token */
            tmp_buf[j++] = ' ';
        }
        else
            tmp_buf[j++] = buf[i];
    }
    tmp_buf[j++] = '\0';
    tokenize_line( tmp_buf, tokens, token_cnt, FALSE );

    if ( *token_cnt < 1 )
        return;

    alias_substitute( tokens, token_cnt );

    strcpy( buf, "" );
    for ( i=0;
            i<*token_cnt;
            i++ )
    {
        if ( i>0 )
            strcat( buf, " " );
        strcat( buf, tokens[i] );
    }
}

/*****************************************************************
 * TAG( alias_substitute )
 *
 * Find and replace aliases in a command line.
 */
static void
alias_substitute( char tokens[MAXTOKENS][TOKENLENGTH], int *token_cnt )
{
    Alias_obj *alias;
    int i, j;

    char *str_delimiter = " ", *token=NULL;

    int  token_string_cnt=0, token_index=0;
    char token_string[MAXTOKENS*TOKENLENGTH];
    Bool_type found_alias=FALSE, match_alias=FALSE;

    /* If the command is an "alias" or "unalias" command, don't do any alias
     * substitutions!
     */
    if ( strcmp( tokens[0], "alias" ) == 0 ||
            strcmp( tokens[0], "unalias" ) == 0 )
        return;

    token_string[0]='\0';

    for ( i = 0;
            i < *token_cnt;
            i++ )
    {
        alias = alias_list;
        match_alias=FALSE;

        while ( alias != NULL )
        {
            if ( strcmp( tokens[i], alias->alias_str ) == 0 )
            {
                found_alias = TRUE;
                match_alias = TRUE;

                for ( j = 0;
                        j < alias->token_cnt;
                        j++ )
                {
                    strcat( token_string, alias->tokens[j] );
                    strcat( token_string, " " );
                }

            }
            alias = alias->next;
        }
        if ( !match_alias )
        {
            strcat( token_string, tokens[i] );
            strcat( token_string, " " );
        }
    }

    if ( strlen(token_string)==0 || !found_alias )
        return;

    token = strtok(token_string, str_delimiter);
    strcpy(tokens[token_index++], token);

    while (1)
    {
        token = strtok(NULL, str_delimiter);
        if ( token==NULL )
            break;
        else
            strcpy(tokens[token_index++], token);

    }
    *token_cnt = token_index;
}

/*****************************************************************
 * TAG( create_reflect_mats )
 *
 * Create transform matrices for the specified reflection plane.
 * The transforms are for reflecting a point and reflecting a
 * normal vector.
 */
void
create_reflect_mats( Refl_plane_obj *plane )
{
    Transf_mat rot_transf, invrot_transf;
    Transf_mat *transf;
    float vec[3], vecy[3], vecz[3];
    int i;

    /*
     * The reflection plane is specified by a point on the plane and
     * the normal to the plane.
     *
     * For transforming a point, we do the following:
     *
     *     get local set of axes, with the plane normal as the X axis
     *         and the Y and Z axes lying in the reflection plane
     *     translate plane pt to origin
     *     rotate local axes to global axes
     *     scale in X by -1
     *     rotate back
     *     translate back
     *
     *     (Remember that poly node ordering is reversed by reflection.)
     *
     * For transforming a normal, the transform is:
     *
     *     rotate local axes to global axes
     *     scale in X by -1
     *     rotate back
     */

    vec_norm( plane->norm );

    /* Get the local axes. */
    mat_copy( &rot_transf, &ident_matrix );
    mat_rotate_axis_vec( &rot_transf, 0, plane->norm, TRUE );
    VEC_SET( vec, 0.0, 1.0, 0.0 );
    point_transform( vecy, vec, &rot_transf );
    VEC_SET( vec, 0.0, 0.0, 1.0 );
    point_transform( vecz, vec, &rot_transf );

    /* Get rotation transforms for local-to-global and global-to-local. */
    mat_copy( &rot_transf, &ident_matrix );
    mat_copy( &invrot_transf, &ident_matrix );

    for ( i = 0; i < 3; i++ )
    {
        rot_transf.mat[i][0] = plane->norm[i];
        rot_transf.mat[i][1] = vecy[i];
        rot_transf.mat[i][2] = vecz[i];
        invrot_transf.mat[0][i] = plane->norm[i];
        invrot_transf.mat[1][i] = vecy[i];
        invrot_transf.mat[2][i] = vecz[i];
    }

    /* Get the point transformation matrix. */
    transf = &plane->pt_transf;
    mat_copy( transf, &ident_matrix );
    mat_translate( transf, -plane->pt[0], -plane->pt[1], -plane->pt[2] );
    mat_mul( transf, transf, &rot_transf );
    mat_scale( transf, -1.0, 1.0, 1.0 );
    mat_mul( transf, transf, &invrot_transf );
    mat_translate( transf, plane->pt[0], plane->pt[1], plane->pt[2] );

    /* Get the normal transformation matrix. */
    transf = &plane->norm_transf;
    mat_copy( transf, &ident_matrix );
    mat_mul( transf, transf, &rot_transf );
    mat_scale( transf, -1.0, 1.0, 1.0 );
    mat_mul( transf, transf, &invrot_transf );
}

/*****************************************************************
 * TAG( is_numeric_token )
 *
 * Evaluate a string and return TRUE if string is an ASCII
 * representation of an integer or floating point value
 * (does NOT recognize scientific notation).
 */
Bool_type
is_numeric_token( char *num_string )
{
    char *p_c;
    int pt_cnt;

    pt_cnt = 0;

    /*
     * Loop through the characters in the string, returning
     * FALSE if any character is not a digit or is not the first
     * decimal point (period) in the string or if the string
     * is empty.
     */
    for ( p_c = num_string; *p_c != '\0'; p_c++ )
    {
        if ( *p_c > '9' )
            return FALSE;
        else if ( *p_c < '0' )
            if ( *p_c != '.' )
                return FALSE;
            else
            {
                pt_cnt++;
                if ( pt_cnt > 1 )
                    return FALSE;
            }
    }

    return (p_c == num_string) ? FALSE : TRUE;
}

/*****************************************************************
 * TAG( is_numeric_range_token )
 *
 * Evaluate a string and return TRUE if string is an ASCII
 * representation of an integer range of the form 99-99.
 */
Bool_type
is_numeric_range_token( char *num_string )
{
    char one_char;
    char num_one[10], num_two[10];
    char temp_string[32];

    int  i;
    int  num_min,     num_max;
    int  pt_cnt;
    Bool_type         dash_found = FALSE;

    pt_cnt = 0;
    strcpy(temp_string, num_string);

    /*
     * Loop through the characters in the string, looking for the '-'
     * that seperates the two numbers, then parse the min mat and
     * max mat.
     */
    for ( i = 1;
            i < strlen(temp_string);
            i++ )
    {
        if ( temp_string[i] == '-' || temp_string[i] == ':' )
        {
            dash_found    = TRUE;
            temp_string[i] = ' ';
        }
    }

    if (dash_found)
    {
        sscanf(temp_string, "%s %s", num_one, num_two);
        if (is_numeric_token(num_one) && is_numeric_token(num_two))
        {
            sscanf(temp_string, "%d %d", &num_min, &num_max);
            if (num_min < num_max)
                return TRUE;
        }
    }
    return FALSE;
}

/*****************************************************************
 * TAG( get_prake_data )
 *
 * Assign prake data items from input tokens
 *
 */
static int
get_prake_data ( int token_count, char prake_tokens[][TOKENLENGTH],
                 int *ptr_ival, float pt[], float vec[], float rgb[] )
{
    int count, i;

    /* */

    if ( ((count = sscanf( prake_tokens[1], "%d", ptr_ival )) == EOF ) ||
            (1 != count) )
        popup_dialog( INFO_POPUP,
                      "get_prake_data:  Invalid data:  n:  %d", ptr_ival );

    for ( i = 0; i < 3; i++ )
    {
        if ( ((count = sscanf( prake_tokens[i + 2], "%f", &pt[i] )) == EOF ) ||
                (1 != count) )
            popup_dialog( INFO_POPUP,
                          "get_prake_data:  Invalid data:  pt:  %f", pt[i] );
    }

    for ( i = 0; i < 3; i++ )
    {
        if ( ((count = sscanf( prake_tokens[i + 5], "%f", &vec[i] )) == EOF ) ||
                (1 != count) )
            popup_dialog( INFO_POPUP,
                          "get_prake_data:  Invalid data:  vec:  %f", vec[i] );
    }

    if ( token_count >= 11 )
    {
        for ( i = 0; i < 3; i++ )
        {
            if ( ((count = sscanf( prake_tokens[i+8], "%f", &rgb[i] )) == EOF )
                    || (1 != count) )
                popup_dialog( INFO_POPUP,
                              "get_prake_data:  Invalid data:  rgb:  %f",
                              rgb[i] );
        }
    }
    else
        rgb[0] = -1;

    return ( count );
}

/*****************************************************************
 * TAG( tokenize )
 *
 * Parse string containing prake input data into individual tokens.
 *
 */
/**static**/ int
tokenize( char *ptr_input_string, char token_list[][TOKENLENGTH],
          size_t maxtoken )
{
    int qty_tokens;
    char *ptr_current_token, *ptr_NULL;
    static char token_separators[] = "\t\n ,;";

    /* */

    ptr_NULL = "";

    if ( (ptr_input_string == NULL) || !maxtoken )
        return ( 0 );

    ptr_current_token = strtok( ptr_input_string, token_separators );

    for ( qty_tokens = 0; (size_t) qty_tokens < maxtoken
            && ptr_current_token != NULL; )
    {
        strcpy( token_list[qty_tokens++], ptr_current_token );

        ptr_current_token = strtok ( NULL, token_separators );
    }

    strcpy( token_list[qty_tokens], ptr_NULL );

    return ( qty_tokens );
}

/*****************************************************************
 * TAG( prake )
 *
 * Evaluate and process prake data.  Prake data may be entered
 * directly via terminal or from specified file.
 *
 */
static void
prake( int token_cnt, char tokens[MAXTOKENS][TOKENLENGTH], int *ptr_ival,
       float pt[], float vec[], float rgb[], Analysis *analy )
{
    FILE *prake_file;
    char  buffer [TOKENLENGTH];
    char prake_tokens [MAXTOKENS][TOKENLENGTH];
    int i, prake_token_cnt;
    char *usage1 = "prake n x1 y1 z1 x2 y2 z2 [r g b]";
    char *usage2 = "prake <filename>";

    /* Duplicate token count and tokens subject to local modification. */

    prake_token_cnt = token_cnt;

    for ( i = 0; i < prake_token_cnt; i++ )
        strcpy( prake_tokens[i], tokens[i] );

    if ( prake_token_cnt > 1 )
    {
        if ( 2 == prake_token_cnt )
        {
            /*
             * Obtain prake data from specified input file.
             */

            if ( (prake_file = fopen( prake_tokens[1], "r" )) != NULL )
            {
                while ( (fgets( buffer, sizeof( buffer ), prake_file )) != NULL )
                {
                    /*
                     * NOTE:  Obtain list of prake tokens from input file --
                     *        NOT directly from GRIZ token processing.
                     *
                     *        Omit blank lines or comment lines [denoted
                     *        by "#" in first column of prake data file.
                     *
                     *        Offset tokens for compatability with GRIZ token
                     *        processing in which "tokens[0] = prake".  This
                     *        also requires an additional increment to
                     *        "prake_token_cnt" for consistency with
                     *        GRIZ token processing.
                     */

                    prake_token_cnt = tokenize( buffer, prake_tokens,
                                                (MAXTOKENS - 1) );

                    if ( (0 != prake_token_cnt) &&
                            strstr( prake_tokens[0], "#" ) == NULL )
                    {
                        if ( prake_token_cnt >= 7 )
                        {
                            for ( i = prake_token_cnt; i > -1; i-- )
                                strcpy( prake_tokens[i + 1], prake_tokens[i] );
                            strcpy( prake_tokens[0], "prake" );
                            prake_token_cnt++;
                            if ( get_prake_data( prake_token_cnt, prake_tokens,
                                                 ptr_ival, pt, vec, rgb ) > 0 )
                                gen_trace_points( *ptr_ival, pt, vec, rgb,
                                                  analy );
                            else
                                popup_dialog( USAGE_POPUP,
                                              "%s\n%s", usage1, usage2 );
                        }
                        else
                            popup_dialog( USAGE_POPUP,
                                          "%s\n%s", usage1, usage2 );
                    }
                }
                fclose( prake_file );
            }
            else
                popup_dialog( INFO_POPUP,
                              "prake:  Unable to open file:  %s", tokens[1] );
        }
        else
        {
            /*
             * No prake data file specified.
             *
             * Obtain prake data from GRIZ input tokenizer.
             */

            if ( prake_token_cnt > 7 )
            {
                if ( get_prake_data( prake_token_cnt, prake_tokens, ptr_ival,
                                     pt, vec, rgb ) > 0 )
                    gen_trace_points( *ptr_ival, pt, vec, rgb, analy );
                else
                    popup_dialog( USAGE_POPUP, "%s\n%s",
                                  usage1, usage2 );
            }
            else
                popup_dialog( USAGE_POPUP, "%s\n%s", usage1, usage2 );
        }
    }
    else
        popup_dialog( USAGE_POPUP, "%s\n%s", usage1, usage2 );

    return;
}

/*****************************************************************
 * TAG( outvec )
 *
 * Prepare ASCII dump of vector grid values
 *
 */
static void
outvec( char outvec_file_name[TOKENLENGTH], Analysis *analy )
{
    FILE *outvec_file;

    /*
     * Test to determine if vgrid"N" and vector qualifiers have
     * been established.
     */
    if ( !analy->vectors_at_nodes && !analy->have_grid_points )
    {
        popup_dialog( INFO_POPUP, "outvec: No vector points exist." );
        return;
    }

    if ( (analy->vector_result[0] == NULL)  &&
            (analy->vector_result[1] == NULL)  &&
            (analy->vector_result[2] == NULL))
    {
        popup_dialog( INFO_POPUP, "No vector result has been set.\n%s"
                      "Use  \"vec <vx> <vy> <vz>\"." );

        return;
    }

    if ( (outvec_file = fopen( (char *) outvec_file_name, "w" )) != NULL )
    {
        write_preamble( outvec_file );
        write_outvec_data( outvec_file, analy );
        fclose( outvec_file );
    }
    else
        popup_dialog( INFO_POPUP, "outvec:  Unable to open file:  %s",
                      outvec_file_name );

    return;
}

/*****************************************************************
 * TAG( write_preamble )
 *
 * Write information preamble to output data file.
 */
void
write_preamble( FILE *ofile )
{
    char
    *date_label   = "Date: ",
     *file_creator = "Output file created by: ",
      *infile_label = "Input file: ";

    fprintf( ofile, "#\n# %s%s\n# %s%s\n#\n# %s%s\n#\n",
             date_label, env.date,
             file_creator, env.user_name,
             infile_label, env.plotfile_name );

    fprintf( ofile, "#---------------------------------------------------------------" );
    fprintf( ofile, "\n#---------------------- Griz Version Info ----------------------" );
    fprintf( ofile, "\n#" );
    fprintf( ofile, "\n# Griz Version %s.%s.%s", GRIZ_MAJOR, GRIZ_MINOR, GRIZ_BUG);
    fprintf( ofile, "\n# Built: %s", bi_date());
    fprintf( ofile, "\n#        on - %s",bi_system());
    fprintf( ofile, "\n#        by - %s",bi_developer());
    fprintf( ofile, "\n#---------------------------------------------------------------" );
    return;
}

/*****************************************************************
 * TAG( write_outvec_data )
 *
 * Output ASCII file of nodal/vector coordinates and vector values
 */

#define PRECISION 6

static void
write_outvec_data( FILE *outvec_file, Analysis *analy )
{
    Vector_pt_obj *point;
    char
    *comment_label  = "#  "
                      ,*quantity_label = "Quantity of samples: "
                                         ,*space          = "   "
                                                 ,*x_label        = "X"
                                                         ,*y_label        = "Y"
                                                                 ,*z_label        = "Z";
    int
    length_comment
    ,length_space
    ,length_vector_x_label
    ,length_vector_y_label
    ,length_vector_z_label
    ,length_x_label
    ,length_y_label
    ,length_z_label
    ,number_width
    ,width_column_1
    ,width_column_2
    ,width_column_3
    ,width_column_4
    ,width_column_5
    ,width_column_6;
    int qty_of_nodes;
    int i, j;
    float *vec_data[3];
    Result *tmp_result;
    float *tmp_data;
    GVec3D *nodes3d;
    GVec2D *nodes2d;
    char *titles[3];
    int class_qty;
    MO_class_data **p_mo_classes;
    MO_class_data *p_node_class;

    for ( i = 0; i < 3; i++ )
    {
        if ( analy->vector_result[i] != NULL )
            griz_str_dup( titles + i, analy->vector_result[i]->title );
        else
            griz_str_dup( titles + i, "Zero" );
    }

    number_width = PRECISION + 7;

    length_comment = strlen( comment_label );
    length_space   = strlen( space );
    length_x_label = strlen ( x_label );
    width_column_1 = length_space +
                     ( ( number_width > length_x_label ) ?
                       number_width : length_x_label );
    length_y_label = strlen ( y_label );
    width_column_2 = length_space +
                     ( ( number_width > length_y_label ) ?
                       number_width : length_y_label );

    p_node_class = MESH_P( analy )->node_geom;
    qty_of_nodes = p_node_class->qty;

    if ( 3 == analy->dimension )
    {
        length_z_label = strlen ( z_label );
        width_column_3 = length_space +
                         ( ( number_width > length_z_label ) ?
                           number_width : length_z_label );

        length_vector_x_label = strlen ( titles[0] );
        width_column_4 = length_space +
                         ( ( number_width > length_vector_x_label ) ?
                           number_width : length_vector_x_label );

        length_vector_y_label = strlen ( titles[1] );
        width_column_5 = length_space +
                         ( ( number_width > length_vector_y_label ) ?
                           number_width : length_vector_y_label );

        length_vector_z_label = strlen ( titles[2] );
        width_column_6 = length_space +
                         ( ( number_width > length_vector_z_label ) ?
                           number_width : length_vector_z_label );

        if ( analy->vectors_at_nodes )
        {
            /* switch nodvec is set; Complete nodal data set:  3D */

            for ( i = 0; i < 3; i++ )
            {
                vec_data[i] = NEW_N( float, qty_of_nodes, "Vec comp result" );

                if ( vec_data[i] == NULL )
                {
                    for ( j = i - 1; j >= 0; j-- )
                        free( vec_data[j] );
                    popup_dialog( WARNING_POPUP, "%s\n%s",
                                  "Memory allocation for vector result failed.",
                                  "\"outvec\" processing aborted." );
                    return;
                }
            }

            /* Print column headings */
            fprintf ( outvec_file,
                      "#\n#\n# %s%d\n#\n#%s%*s%s%*s%s%*s%s%*s%s%*s%s%*s\n#\n",
                      quantity_label, qty_of_nodes, comment_label,
                      width_column_1 - length_comment, x_label, space,
                      width_column_2 - length_space, y_label, space,
                      width_column_3 - length_space, z_label, space,
                      width_column_4 - length_space, titles[0], space,
                      width_column_5 - length_space, titles[1], space,
                      width_column_6 - length_space, titles[2] );

            /* Load data */

            for ( i = 0; i < 3; i++ )
            {
                if ( analy->vector_result[i] == NULL )
                    continue;
                update_result( analy, analy->vector_result[i] );
            }

            tmp_result = analy->cur_result;
            tmp_data = p_node_class->data_buffer;

            for ( i = 0; i < 3; i++ )
            {
                if ( analy->vector_result[i] == NULL )
                    continue;
                analy->cur_result = analy->vector_result[i];
                p_node_class->data_buffer = vec_data[i];
                load_result( analy, TRUE, TRUE, FALSE );
            }

            analy->cur_result = tmp_result;
            p_node_class->data_buffer = tmp_data;

            nodes3d = analy->state_p->nodes.nodes3d;

            /* Print data */
            for ( i = 0; i < qty_of_nodes; i++ )
                fprintf ( outvec_file,
                          "% *.*e% *.*e% *.*e% *.*e % *.*e% *.*e\n",
                          width_column_1, PRECISION, nodes3d[i][0],
                          width_column_2, PRECISION, nodes3d[i][1],
                          width_column_3, PRECISION, nodes3d[i][2],
                          width_column_4, PRECISION, vec_data[0][i],
                          width_column_5, PRECISION, vec_data[1][i],
                          width_column_6, PRECISION, vec_data[2][i] );

            for ( i = 0; i < 3; i++ )
                free( vec_data[i] );
        }
        else
        {
            /* switch grdvec is set; Specified nodal data subset:  3D */

            /* Count the grid points. */
            qty_of_nodes = 0;

#ifdef NEWMILI
            htable_get_data( MESH( analy ).class_table,
                             (void ***) &p_mo_classes ,
                             &class_qty);
#else
            class_qty = htable_get_data( MESH( analy ).class_table,
                                         (void ***) &p_mo_classes );
#endif

            for ( i = 0; i < class_qty; i++ )
                qty_of_nodes += p_mo_classes[i]->vector_pt_qty;

            /*
             * NOTE:  "update_vec_points" will only update vector components
             *        if the "show_vectors" flag is set == TRUE
             */

            if ( analy->show_vectors )
                update_vec_points( analy );
            else
            {
                analy->show_vectors = TRUE;
                update_vec_points( analy );
                analy->show_vectors = FALSE;
            }

            /* Print column headings */
            fprintf ( outvec_file,
                      "#\n#\n# %s%d\n#\n%s%*s%s%*s%s%*s%s%*s%s%*s%s%*s\n#\n",
                      quantity_label, qty_of_nodes, comment_label,
                      width_column_1 - length_comment, x_label, space,
                      width_column_2 - length_space, y_label, space,
                      width_column_3 - length_space, z_label, space,
                      width_column_4 - length_space, titles[0], space,
                      width_column_5 - length_space, titles[1], space,
                      width_column_6 - length_space, titles[2] );

            /* Print data */
            for ( i = 0; i < class_qty; i++ )
            {
                for ( point = p_mo_classes[i]->vector_pts; point != NULL;
                        NEXT( point ) )
                    fprintf( outvec_file,
                             "% *.*e% *.*e% *.*e% *.*e % *.*e% *.*e\n",
                             width_column_1, PRECISION, point->pt[0],
                             width_column_2, PRECISION, point->pt[1],
                             width_column_3, PRECISION, point->pt[2],
                             width_column_4, PRECISION, point->vec[0],
                             width_column_5, PRECISION, point->vec[1],
                             width_column_6, PRECISION, point->vec[2] );
            }

            free( p_mo_classes );
        }
    }
    else if ( 2 == analy->dimension )
    {
        length_vector_x_label = strlen ( titles[0] );
        width_column_3 = length_space +
                         ( ( number_width > length_vector_x_label ) ?
                           number_width : length_vector_x_label );

        length_vector_y_label = strlen ( titles[1] );
        width_column_4 = length_space +
                         ( ( number_width > length_vector_y_label ) ?
                           number_width : length_vector_y_label );

        if ( analy->vectors_at_nodes )
        {
            /* switch nodvec is set; Complete nodal data set:  2D */

            for ( i = 0; i < 2; i++ )
            {
                vec_data[i] = NEW_N( float, qty_of_nodes, "Vec comp result" );

                if ( vec_data[i] == NULL )
                {
                    for ( j = i - 1; j >= 0; j-- )
                        free( vec_data[j] );
                    popup_dialog( WARNING_POPUP, "%s\n%s",
                                  "Memory allocation for vector result failed.",
                                  "\"outvec\" processing aborted." );
                    return;
                }
            }

            /* Print column headings */
            fprintf ( outvec_file,
                      "#\n#\n# %s%d\n#\n%s%*s%s%*s%s%*s%s%*s\n#\n",
                      quantity_label, qty_of_nodes, comment_label,
                      width_column_1 - length_comment, x_label, space,
                      width_column_2 - length_space, y_label, space,
                      width_column_3 - length_space, titles[0], space,
                      width_column_4 - length_space, titles[1] );

            /* Load data */

            tmp_result = analy->cur_result;
            tmp_data = p_node_class->data_buffer;

            for ( i = 0; i < 2; i++ )
            {
                if ( analy->vector_result[i] == NULL )
                    continue;
                analy->cur_result = analy->vector_result[i];
                p_node_class->data_buffer = vec_data[i];
                load_result( analy, TRUE, TRUE, FALSE );
            }

            analy->cur_result = tmp_result;
            p_node_class->data_buffer = tmp_data;

            nodes2d = analy->state_p->nodes.nodes2d;

            /* Print data */
            for ( i = 0; i < qty_of_nodes; i++ )
                fprintf ( outvec_file,
                          "% *.*e% *.*e% *.*e% *.*e\n",
                          width_column_1, PRECISION, nodes2d[i][0],
                          width_column_2, PRECISION, nodes2d[i][1],
                          width_column_3, PRECISION, vec_data[0][i],
                          width_column_4, PRECISION, vec_data[1][i] );
        }
        else
        {
            /* switch grdvec is set; Specified nodal data subset:  2D */


            /* Count the grid points. */
            qty_of_nodes = 0;

#ifdef NEWMILI
            htable_get_data( MESH( analy ).class_table,
                             (void ***) &p_mo_classes ,
                             &class_qty );
#else
            class_qty = htable_get_data( MESH( analy ).class_table,
                                         (void ***) &p_mo_classes );
#endif

            for ( i = 0; i < class_qty; i++ )
                qty_of_nodes += p_mo_classes[i]->vector_pt_qty;

            /*
             * NOTE:  "update_vec_points" will only update vector components
             *        if the "show_vectors" flag is set == TRUE
             */

            if ( analy->show_vectors )
                update_vec_points( analy );
            else
            {
                analy->show_vectors = TRUE;
                update_vec_points( analy );
                analy->show_vectors = FALSE;
            }

            /* Print column headings */
            fprintf ( outvec_file,
                      "#\n#\n# %s%d\n#\n%s%*s%s%*s%s%*s%s%*s\n#\n",
                      quantity_label, qty_of_nodes, comment_label,
                      width_column_1 - length_comment, x_label, space,
                      width_column_2 - length_space, y_label, space,
                      width_column_3 - length_space, titles[0], space,
                      width_column_4 - length_space, titles[1] );

            for ( i = 0; i < class_qty; i++ )
            {
                for ( point = p_mo_classes[i]->vector_pts; point != NULL;
                        NEXT( point ) )
                    fprintf( outvec_file,
                             "% *.*e% *.*e% *.*e% *.*e\n",
                             width_column_1, PRECISION, point->pt[0],
                             width_column_2, PRECISION, point->pt[1],
                             width_column_3, PRECISION, point->vec[0],
                             width_column_4, PRECISION, point->vec[1] );
            }

            free( p_mo_classes );
        }
    }

    for ( i = 0; i < 3; i++ )
        free( titles[i] );

    return;
}

/*****************************************************************
 * TAG( start_text_history )
 *
 * Prepare ASCII file of text history
 */
static void
start_text_history( char text_history_file_name[] )
{
    if ( (text_history_file = fopen( text_history_file_name, "w" )) != NULL )
        text_history_invoked = TRUE;
    else
    {
        popup_dialog( INFO_POPUP, "text_history:  Unable to open file:  %s",
                      text_history_file_name );
        text_history_invoked = FALSE;
    }

    return;
}

/*****************************************************************
 * TAG( end_text_history )
 *
 * Prepare ASCII file of text history
 */
static void
end_text_history( void )
{
    fclose( text_history_file );
    text_history_invoked = FALSE;

    return;
}

/*****************************************************************
 * TAG( check_for_result )
 *
 * Prepare ASCII file of text history
 */
static int
check_for_result( Analysis *analy, int display_warning )
{
    int valid_result = TRUE;

    if ( analy->cur_result == NULL)
    {
        if ( display_warning )
            popup_dialog( WARNING_POPUP, "No result selected.");

        valid_result = FALSE;
    }
    return( valid_result );
}

/*****************************************************************
 * TAG( dump_tokens )
 *
 * This function will write out the contents of the input tokens.
 */
void
dump_tokens ( int token_cnt,
              char tokens[MAXTOKENS][TOKENLENGTH] )
{
    int i=0;
    printf("\n");
    for ( i=0;
            i<token_cnt;
            i++ )
    {
        printf("\nToken[%i] = %s", i, tokens[i] );
    }
    printf("\n");
}

/*****************************************************************
 * TAG(mat_range)
 *
 * This function will process the given token list and substitute
 * any tokens containing material names for their corresponding
 * numbers
 */
void
mat_range(Analysis *analy, char *token1, char *token2, int nums[analy->max_mesh_mat_qty], int *token_cnt){
	Htable_entry *tempEnt;
	//int *nums = malloc();
	*token_cnt = 0;
	Bool_type failure = False;
	//
	Bool_type found1 = False;
	int pos1 = -1;
	Bool_type found2 = False;
	int pos2 = -1;
	int pos = 0;


	//search list for names
	for(pos = 0; pos < analy->max_mesh_mat_qty; pos++){
		if(strcmp(analy->sorted_labels[pos],token1) == 0){
			found1 = True;
			pos1 = pos;
		}
		if(strcmp(analy->sorted_labels[pos],token2) == 0){
			found2 = True;
			pos2 = pos;
		}
	}

	if(found1 && found2){
		int begin = 0;
		int end = 0;
		int loc = 0;
		//are they in proper order?
		if(pos2 >= pos1){
			begin = pos1;
			end = pos2;
		}
		//reverse order if needed
		else{
			begin = pos2;
			end = pos1;
		}
		pos = 0;
		for(loc = begin; loc <= end; loc++){
			htable_search(analy->mat_labels,analy->sorted_labels[loc],FIND_ENTRY,&tempEnt);
			nums[pos] = atoi((char*)tempEnt->data);
			pos++;
		}
		*token_cnt = pos;
	}
	//nums now contains the list of all the material numbers for names between entry 1 and 2
}

/*****************************************************************
 * TAG(mat_name_sub)
 *
 * This function will process the given token list and substitute
 * any tokens containing material names for their corresponding
 * numbers
 */
int
mat_name_sub(Analysis *analy, char tokens[MAXTOKENS][TOKENLENGTH], int *token_cnt){
	int result = 1;
	int namepos,tokenpos,i = 0;
	Htable_entry *tempEnt;
	char new_tokens[MAXTOKENS][TOKENLENGTH];
	int new_token_cnt = 0;
    //do we at least have a command and 1 argument
	if(*token_cnt > 1){
		char token[TOKENLENGTH];
		sprintf(token, "%s",tokens[0]);
		if(	(strcmp(tokens[0],"include") == 0) 	|| (strcmp(tokens[0],"exclude") == 0) 	||
			(strcmp(tokens[0],"vis") == 0) 		|| (strcmp(tokens[0],"invis") == 0) 	||
			(strcmp(tokens[0],"enable") == 0) 	|| (strcmp(tokens[0],"disable") == 0) 	||
			(strcmp(tokens[0],"hilite") == 0) 	|| (strcmp(tokens[0],"select") == 0)	||
			(strcmp(tokens[0],"set_ipt") == 0)	){
			Bool_type dash_found = False;
			Bool_type next_has_dash = False;
			Bool_type hunting = True;

			char temp[TOKENLENGTH];
			strcpy(new_tokens[0], tokens[0]);
			new_token_cnt = 1;
			tokenpos = 1;
			if(strcmp(tokens[0],"set_ipt") == 0){
				strcpy(new_tokens[1], tokens[1]);
				new_token_cnt = 2;
				tokenpos = 2;
			}

			//skip duplicate commands if vis or enable related command
			while(	(strcmp(tokens[tokenpos],"vis") == 0) 		|| (strcmp(tokens[tokenpos],"invis") == 0) 		||
					(strcmp(tokens[tokenpos],"enable") == 0) 	|| (strcmp(tokens[tokenpos],"disable") == 0)	){
				strcpy(new_tokens[tokenpos], tokens[tokenpos]);
				new_token_cnt++;
				tokenpos++;
			}

			for(tokenpos; tokenpos < *token_cnt; tokenpos++){
				char tempToken[TOKENLENGTH];
				strcpy(tempToken,tokens[tokenpos]);
				//should we be hunting for mat names
				if(is_classname(tempToken, MESH( analy ).class_table)){
					if((strcmp(tokens[tokenpos],"mat") == 0) || (strcmp(tokens[tokenpos],"MAT") == 0)){
						hunting = True;
					}
					else{
						hunting = False;
					}
				}
				//other reserved words to not be substituted
				else{
					if(	(strcmp(tempToken,"all") == 0) || (strcmp(tempToken,"allb") == 0) ||
						(strcmp(tempToken,"amb") == 0) || (strcmp(tempToken,"diff") == 0) ||
						(strcmp(tempToken,"spec") == 0) || (strcmp(tempToken,"gslevel") == 0) ||
						(strcmp(tempToken,"emis") == 0) || (strcmp(tempToken,"shine") == 0) ||
						(strcmp(tempToken,"preview") == 0) || (strcmp(tempToken,"cancel") == 0) ||
						(strcmp(tempToken,"apply") == 0) || (strcmp(tempToken,"default") == 0)){
						hunting = False;
					}
				}

				if(hunting){
					//does our token contain a dash?
					dash_found = (strstr(tokens[tokenpos],"-") != NULL);
					//does our next token(if any) contain a dash
					if(tokenpos+1 < *token_cnt){
						next_has_dash = (strstr(tokens[tokenpos+1],"-") != NULL);
					}
					char tokholder[TOKENLENGTH];
					//is there a dash of any kind?
					if(dash_found || next_has_dash){
						//if dash found in first item
						if(dash_found){
							//if first item begins with dash, we have hit an invalid case
							if( strncmp(tokens[tokenpos],"-",1) == 0 ){
								//process only 1 token
								strcpy(new_tokens[new_token_cnt], tokens[tokenpos]);
								new_token_cnt+=1;
							}
							//we have to check further
							else{
								//does first token end with dash?
								if( strcmp(tokens[tokenpos]+(strlen(tokens[tokenpos])-1),"-") == 0 ){
									//merge 2 tokens
									sprintf(tokholder,"%s%s",tokens[tokenpos],tokens[tokenpos+1]);
									//handle range
									char *token;
									token = strtok(tokholder,"-");
									char *firstName = token;
									token = strtok(NULL,"-");
									char *secondName = token;
									int nums[analy->max_mesh_mat_qty];
									int numnums = 0;
									Bool_type token1str = False;
									Bool_type token2str = False;
									htable_search(analy->mat_labels,firstName,FIND_ENTRY,&tempEnt);
									token1str = (tempEnt != NULL);
									htable_search(analy->mat_labels,secondName,FIND_ENTRY,&tempEnt);
									token2str = (tempEnt != NULL);
									if(token1str && token2str){
										mat_range(analy,firstName,secondName,nums,&numnums);
										int pos = 0;
										for (pos = 0; pos < numnums; pos++){
											sprintf(new_tokens[new_token_cnt],"%i",nums[pos]);
											new_token_cnt +=1;
										}
									}//
									else{
										strcpy(new_tokens[new_token_cnt], tokens[tokenpos]);
										strcpy(new_tokens[new_token_cnt+1], tokens[tokenpos+1]);
										new_token_cnt+=2;
										tokenpos += 1;
									}

								}
								//if not we have a full range in one token
								else{
									//no merge necessary
									sprintf(tokholder,"%s",tokens[tokenpos]);
									//handle range
									char *token;
									token = strtok(tokholder,"-");
									char *firstName = token;
									token = strtok(NULL,"-");
									char *secondName = token;
									int nums[analy->max_mesh_mat_qty];
									int numnums = 0;
									Bool_type token1str = False;
									Bool_type token2str = False;
									htable_search(analy->mat_labels,firstName,FIND_ENTRY,&tempEnt);
									token1str = (tempEnt != NULL);
									htable_search(analy->mat_labels,secondName,FIND_ENTRY,&tempEnt);
									token2str = (tempEnt != NULL);
									if(token1str && token2str){
										mat_range(analy,firstName,secondName,nums,&numnums);
										int pos = 0;
										for (pos = 0; pos < numnums; pos++){
											sprintf(new_tokens[new_token_cnt],"%i",nums[pos]);
											new_token_cnt +=1;
										}
									}
									else{
										strcpy(new_tokens[new_token_cnt], tokens[tokenpos]);
										new_token_cnt+=1;
									}
								}
							}
						}
						//otherwise dash is in the second item
						else{
							//is second token only a dash?
							if(strcmp(tokens[tokenpos+1],"-") == 0){
								//merge 3 tokens
								sprintf(tokholder,"%s%s%s",tokens[tokenpos],tokens[tokenpos+1],tokens[tokenpos+2]);
								//handle range
								char *token;
								token = strtok(tokholder,"-");
								char *firstName = token;
								token = strtok(NULL,"-");
								char *secondName = token;
								int nums[analy->max_mesh_mat_qty];
								int numnums = 0;
								Bool_type token1str = False;
								Bool_type token2str = False;
								htable_search(analy->mat_labels,firstName,FIND_ENTRY,&tempEnt);
								token1str = (tempEnt != NULL);
								htable_search(analy->mat_labels,secondName,FIND_ENTRY,&tempEnt);
								token2str = (tempEnt != NULL);
								if(token1str && token2str){
									mat_range(analy,firstName,secondName,nums,&numnums);
									int pos = 0;
									for (pos = 0; pos < numnums; pos++){
										sprintf(new_tokens[new_token_cnt],"%i",nums[pos]);
										new_token_cnt +=1;
									}
								}
								else{
									strcpy(new_tokens[new_token_cnt], tokens[tokenpos]);
									strcpy(new_tokens[new_token_cnt+1], tokens[tokenpos+1]);
									strcpy(new_tokens[new_token_cnt+2], tokens[tokenpos+2]);
									new_token_cnt+=3;
									tokenpos += 2;
								}
							}
							else{
								//does second token begin with a dash?
								if(strncmp(tokens[tokenpos+1],"-",1) == 0){
									//merge 2 tokens
									sprintf(tokholder,"%s%s",tokens[tokenpos],tokens[tokenpos+1]);
									//handle range
									char *token;
									token = strtok(tokholder,"-");
									char *firstName = token;
									token = strtok(NULL,"-");
									char *secondName = token;
									int nums[analy->max_mesh_mat_qty];
									int numnums = 0;
									Bool_type token1str = False;
									Bool_type token2str = False;
									htable_search(analy->mat_labels,secondName,FIND_ENTRY,&tempEnt);
									token2str = (tempEnt != NULL);
									htable_search(analy->mat_labels,firstName,FIND_ENTRY,&tempEnt);
									token1str = (tempEnt != NULL);
									if(token1str && token2str){
										mat_range(analy,firstName,secondName,nums,&numnums);
										int pos = 0;
										for (pos = 0; pos < numnums; pos++){
											sprintf(new_tokens[new_token_cnt],"%i",nums[pos]);
											new_token_cnt +=1;
										}
									}
									else{
										if (token1str){
											strcpy(new_tokens[new_token_cnt], tempEnt->data);
										}
										else{
											strcpy(new_tokens[new_token_cnt], tokens[tokenpos]);
										}
										strcpy(new_tokens[new_token_cnt+1], tokens[tokenpos+1]);
										new_token_cnt+=2;
										tokenpos += 1;
									}
								}
								// next token is not connected to this token
								else{
									//process only 1
									int len = 1;
									//search table
									htable_search(analy->mat_labels,tokens[tokenpos],FIND_ENTRY,&tempEnt);
									if(tempEnt != NULL){
										strcpy(new_tokens[new_token_cnt], tempEnt->data);
									}
									//no match pass through and proceed
									else{
										if(begins_with(tokens[tokenpos],"*") || ends_with(tokens[tokenpos],"*")){
											int num_entries = 0;
											num_entries = htable_key_search(analy->mat_labels, 0, tokens[tokenpos], NULL );
											if (num_entries > 0){
												char **wildcard_list=(char**) malloc( num_entries*sizeof(char *));
												int new_num_entries;
												new_num_entries = htable_key_search(analy->mat_labels, 0, tokens[tokenpos], wildcard_list );
												int pos = 0;
												for (pos = 0; pos < num_entries; pos++){
													htable_search(analy->mat_labels,wildcard_list[pos],FIND_ENTRY,&tempEnt);
													if(tempEnt != NULL){
														strcpy(new_tokens[new_token_cnt], tempEnt->data);
														new_token_cnt +=1;
													}
												}

											}
											else{
												strcpy(new_tokens[new_token_cnt], tokens[tokenpos]);
											}
										}
										else{
											strcpy(new_tokens[new_token_cnt], tokens[tokenpos]);
										}
									}
									new_token_cnt+=1;
								}
							}
						}
					}
					//if not just see about potentially replacing single token then proceed
					else{
						//is the current token a number?
						//if so copy into new tokens as is
						//otherwise check hash table and replace with number to copy as is
						htable_search(analy->mat_labels,tokens[tokenpos],FIND_ENTRY,&tempEnt);
						//new_tokens[new_token_cnt] = (char*)malloc(TOKENLENGTH * sizeof(char));
						if(tempEnt != NULL){
							strcpy(new_tokens[new_token_cnt], tempEnt->data);
						}
						//no match pass through and proceed
						else{
							if(begins_with(tokens[tokenpos],"*") || ends_with(tokens[tokenpos],"*")){
								int num_entries = 0;
								num_entries = htable_key_search(analy->mat_labels, 0, tokens[tokenpos], NULL );
								if (num_entries > 0){
									char **wildcard_list=(char**) malloc( num_entries*sizeof(char *));
									int new_num_entries;
									new_num_entries = htable_key_search(analy->mat_labels, 0, tokens[tokenpos], wildcard_list );
									int pos = 0;
									for (pos = 0; pos < num_entries; pos++){
										htable_search(analy->mat_labels,wildcard_list[pos],FIND_ENTRY,&tempEnt);
										if(tempEnt != NULL){
											strcpy(new_tokens[new_token_cnt], tempEnt->data);
											new_token_cnt +=1;
										}
									}
								}
								else{
									if(is_classname(tokens[tokenpos], MESH( analy ).class_table)){
										strcpy(new_tokens[new_token_cnt], tokens[tokenpos]);
									}
									else{
										strcpy(new_tokens[new_token_cnt],"");
										result = 0;
										wrt_text("\n %s is an invalid argument \n",tokens[tokenpos]);
									}
								}
							}
							else{
								if(is_classname(tokens[tokenpos], MESH( analy ).class_table)){
									strcpy(new_tokens[new_token_cnt], tokens[tokenpos]);
								}
								else{
									strcpy(new_tokens[new_token_cnt],"");
									result = 0;
									wrt_text("\n %s is an invalid argument \n",tokens[tokenpos]);
								}
							}
						}
						new_token_cnt+=1;
					}
				}
				else{
					strcpy(new_tokens[new_token_cnt], tokens[tokenpos]);
					new_token_cnt +=1;
				}

			}
			////////////////////////////////////////////////////////////////////////////////////////////////
			int wrappos = 0;
			for(wrappos = 0; wrappos < new_token_cnt; wrappos ++){
				strcpy(tokens[wrappos],new_tokens[wrappos]);
			}
			*token_cnt = new_token_cnt;
		}
		if(	(strcmp(tokens[0],"mat") == 0) ){
			i = 1;
			while( (i < *token_cnt) &&
					!((strcmp(tokens[i],"amb") == 0) || (strcmp(tokens[i],"diff") == 0) ||
					(strcmp(tokens[i],"spec") == 0) || (strcmp(tokens[i],"shine") == 0) ||
					(strcmp(tokens[i],"emis") == 0) || (strcmp(tokens[i],"alpha") == 0))){
				htable_search(analy->mat_labels,tokens[i],FIND_ENTRY,&tempEnt);
				//new_tokens[new_token_cnt] = (char*)malloc(TOKENLENGTH * sizeof(char));
				if(tempEnt != NULL){
					strcpy(tokens[i], tempEnt->data);
				}
				i++;
			}
			//no match pass through and proceed
		}
		// otherwise no supstitution needed
	}
	return result;
}

/*****************************************************************
 * TAG( process_mat_obj_selection )
 *
 * This function will process the material/object selection
 * requests for vis/invis, enable/disable, and include/exclude
 * commands.
 */
void
process_mat_obj_selection ( Analysis *analy, char tokens[MAXTOKENS][TOKENLENGTH],
                            int idx, int token_cnt, int mat_qty,
                            int elem_qty, MO_class_data *p_class,
                            unsigned char *p_elem, int  *p_hide_qty, int *p_hide_elem_qty,
                            unsigned char *p_mat,
                            unsigned char setval,
                            Bool_type vis_selected, Bool_type mat_selected )

{
    int i, j, k, ival;
    int mtl, *p_mtl;
    int mat_min, mat_max;

    char tmp_token[1000];
    unsigned char local_setval;

    if ( (idx+1) < token_cnt)
        string_to_upper( tokens[idx+1], tmp_token ); /* Make case insensitive */
    else
        strcpy( tmp_token, "ALL" );

    local_setval = setval;

    /* Material based selection */
    if ( p_mat != NULL && mat_selected )
    {
        if ( strcmp( tmp_token, "ALL" ) == 0  )
        {
            for ( i = 0; i < mat_qty; i++ )
            {
                p_mat[i]  = (unsigned char) setval; /* Hide the selected material */

                if (setval)
                    (*p_hide_qty)++;
                else
                    (*p_hide_qty)--;
            }
        }
        else
        {
            if ( strcmp( tmp_token, "ALLB" ) == 0 || strcmp( tmp_token, "ALLBUT" ) == 0 )
            {
                for ( i = 0; i < mat_qty; i++ )
                {
                    p_mat[i]  = (unsigned char) setval; /* Hide the selected material */

                    if (setval)
                        (*p_hide_qty)++;
                    else
                        (*p_hide_qty)--;
                }
                local_setval = !setval;
            }
            for ( i = 1;
                    i < token_cnt - idx;
                    i++ )
            {
            	if(strcmp( tokens[i], "" ) == 0){
            		continue;
            	}
            	parse_mtl_range(tokens[i+idx], mat_qty, &mat_min, &mat_max ) ;

                mtl = mat_min ;

                for (mtl  = mat_min ;
                        mtl <= mat_max ;
                        mtl++)
                {
                    if (mtl > 0 && mtl <= mat_qty)
                    {
                        p_mat[mtl-1]  = (unsigned char) local_setval; /* Hide the selected material */

                        if ( local_setval )
                            (*p_hide_qty)++;
                        else
                            (*p_hide_qty)--;
                    }
                }
            }
        }
    }
    else /* Object selected - hide by element for a selected material */
        if ( p_class )
        {
            if(p_elem == NULL)
            {
                return;
            }
            p_mtl = p_class->objects.elems->mat;
            if ( strcmp( tmp_token, "ALL" ) == 0  )
            {
                parse_mtl_range("ALL", mat_qty, &mat_min, &mat_max);

                for ( j=0;
                        j<elem_qty;
                        j++ )
                    for (i=mat_min;
                            i<=mat_max;
                            i++)
                    {
                        if ( p_mtl[j] == i-1 )
                            if ( !vis_selected )
                            {
                                p_elem[j] = TRUE;
                                (*p_hide_elem_qty)++;
                            }
                            else
                            {
                                p_elem[j] = FALSE;
                                (*p_hide_elem_qty)--;
                            }
                    }
            }
            else
                for ( i = 1;
                        i < token_cnt - idx;
                        i++ )
                {
                	if(strcmp( tokens[i], "" ) == 0){
						continue;
					}
                    parse_mtl_range(tokens[i+idx], mat_qty, &mat_min, &mat_max) ;

                    for ( j=0;
                            j<elem_qty;
                            j++ )
                        for (k=mat_min; /* changed i to k here */
                                k<=mat_max;
                                k++)
                        {
                            if ( p_mtl[j] == k-1 )
                                if ( !vis_selected )
                                {
                                    p_elem[j] = TRUE;
                                    (*p_hide_elem_qty)++;
                                }
                                else
                                {
                                    p_elem[j] = FALSE;
                                    (*p_hide_elem_qty)--;
                                }
                        }
                }
        }
}

/*****************************************************************
 * TAG( process_node_selection )
 *
 * This function will disable/enable nodes for elements that will
 * included/excluded. If a node is shared with both an included and
 * excluded element then the node is not included.
 */
void
process_node_selection ( Analysis *analy )
{
    Mesh_data     *p_mesh;
    MO_class_data **mo_classes;
    MO_class_data *p_class;
    MO_class_data *p_node_geom;

    int i, j, k;
    int elem_id=0;
    int qty_classes=0;
    int qty_elems, qty_nodes=0, num_connects=0;
    int nd=0;
    int *mats=NULL;
    int (*connects_hex)[8], (*connects_quad)[4], (*connects_beam)[3], (*connects_tet)[3];

    int *disable_mtl;

    int disable_mat=FALSE, disable_elem=FALSE;
    Bool_type particle_class=FALSE;

    p_mesh      = MESH_P( analy );
    p_node_geom = p_mesh->node_geom;
    qty_nodes   = p_node_geom->qty;

    /* First set all valid nodes (nodes that are connected to elements) to
     * false.
     */

    /* This code is disabled by setting p_mesh->disable_nodes_init to TRUE for now.
     * This is to address a special case where unattached nodes are referenced in
     * results.
     */
    if ( !p_mesh->disable_nodes_init )
    {

        /* Set all nodes to be disabled */
        for ( nd = 0;
                nd < qty_nodes;
                nd++ )
        {
            p_mesh->disable_nodes[nd] = TRUE;
        }

        /******************************/
        /* Capture all Included Nodes */
        /******************************/

        /* Hex element classes. */

        qty_classes = p_mesh->classes_by_sclass[G_HEX].qty;
        mo_classes  = (MO_class_data **) p_mesh->classes_by_sclass[G_HEX].list;

        for ( i = 0;
                i < qty_classes;
                i++ )
        {
            p_class      =  mo_classes[i];
            particle_class = is_particle_class( analy, p_class->superclass, p_class->short_name );
            qty_elems    = p_class->qty;
            qty_nodes    = p_node_geom->qty;
            num_connects  = qty_connects[G_HEX];
            connects_hex = (int (*) [num_connects]) p_class->objects.elems->nodes;
            mats         = p_class->objects.elems->mat;

            for ( j = 0;
                    j < qty_elems;
                    j++ )
            {

                elem_id = j;
                disable_mat  = p_mesh->hide_material[ mats[elem_id] ];
                disable_elem = disable_by_object_type( p_class, mats[elem_id], j, analy, NULL );

                if ( particle_class )
                    disable_elem = disable_by_object_type( p_class, mats[elem_id], j, analy, NULL );
                else
                    disable_elem = disable_by_object_type( p_class, mats[elem_id], j, analy, NULL );

                /* Ignore disabled materials. */
                if ( !disable_mat && !disable_elem )
                    for ( k = 0;
                            k < num_connects;
                            k++ )
                    {
                        nd = connects_hex[elem_id][k];
                        p_mesh->disable_nodes[nd] = FALSE;
                    }
            }
        } /* Hexes */

        /* Shell element classes. */
        qty_classes = p_mesh->classes_by_sclass[G_QUAD].qty;
        mo_classes  = (MO_class_data **) p_mesh->classes_by_sclass[G_QUAD].list;

        for ( i = 0;
                i < qty_classes;
                i++ )
        {
            p_class       =  mo_classes[i];
            qty_elems     = p_class->qty;
            num_connects  = qty_connects[G_QUAD];
            connects_quad = (int (*) [num_connects]) p_class->objects.elems->nodes;
            mats          = p_class->objects.elems->mat;

            for ( j = 0;
                    j < qty_elems;
                    j++ )
            {

                elem_id = j;

                /* Ignore disabled materials. */
                if ( !p_mesh->hide_material[ mats[elem_id] ] &&
                        !disable_by_object_type( p_class, mats[elem_id], j, analy, NULL ) )
                    for ( k = 0;
                            k < num_connects;
                            k++ )
                    {
                        nd = connects_quad[elem_id][k];
                        p_mesh->disable_nodes[nd] = FALSE;
                    }
            }
        } /* Shells */

        /* Beam element classes. */
        qty_classes = p_mesh->classes_by_sclass[G_BEAM].qty;
        mo_classes  = (MO_class_data **) p_mesh->classes_by_sclass[G_BEAM].list;

        for ( i = 0;
                i < qty_classes;
                i++ )
        {
            p_class       =  mo_classes[i];
            qty_elems     = p_class->qty;
            num_connects  = qty_connects[G_BEAM];
            connects_beam = (int (*) [num_connects]) p_class->objects.elems->nodes;
            mats          = p_class->objects.elems->mat;

            for ( j = 0;
                    j < qty_elems;
                    j++ )
            {

                elem_id = j;

                /* Include  materials & Beams */
                if ( !p_mesh->hide_material[ mats[elem_id] ] &&
                        !disable_by_object_type( p_class, mats[elem_id], j, analy, NULL ) )
                    for ( k = 0;
                            k < num_connects;
                            k++ )
                    {
                        nd = connects_beam[elem_id][k];
                        p_mesh->disable_nodes[nd] = FALSE;
                    }
            }
        } /* Beams */

        /* Tet element classes. */
        qty_classes = p_mesh->classes_by_sclass[G_TET].qty;
        mo_classes  = (MO_class_data **) p_mesh->classes_by_sclass[G_TET].list;

        for ( i = 0;
                i < qty_classes;
                i++ )
        {
            p_class       =  mo_classes[i];
            qty_elems     = p_class->qty;
            num_connects  = qty_connects[G_TET];
            connects_tet  = (int (*) [num_connects]) p_class->objects.elems->nodes;
            mats          = p_class->objects.elems->mat;

            for ( j = 0;
                    j < qty_elems;
                    j++ )
            {

                elem_id = j;

                /* Include  materials & Tets */
                if ( !p_mesh->hide_material[ mats[elem_id] ] &&
                        !disable_by_object_type( p_class, mats[elem_id], j, analy, NULL ) )
                    for ( k = 0;
                            k < num_connects;
                            k++ )
                    {
                        nd = connects_tet[elem_id][k];
                        p_mesh->disable_nodes[nd] = FALSE;
                    }
            }
        } /* Tets */
    }
}

/*****************************************************************
 * TAG( is_node_visible() )
 *
 * This function will return status of nodes visibility.
 */
int
is_node_visible( Analysis *analy, int nd )
{
    Mesh_data     *p_mesh;
    MO_class_data *p_node_geom;
    int qty_nodes;

    p_mesh      = MESH_P( analy );
    p_node_geom = p_mesh->node_geom;
    qty_nodes   = p_node_geom->qty;

    if ( nd<0 || nd>=qty_nodes )
        return( -1 );
    if ( p_mesh->disable_nodes[nd] )
        return( 0 );
    else
        return( 1 );
}

/*****************************************************************
 * TAG( is_elem_mat_visible() )
 *
 * This function will return status of an elements material
 * visibility.
 */
Bool_type
is_elem_mat_visible( Analysis *analy, int elem_id, MO_class_data *p_mo_class )
{
    int *mtl_list=NULL, material=0;
    int qty=0;
    Mesh_data     *p_mesh;
    unsigned char *hide_mtl;

    qty         = p_mo_class->qty;
    if ( elem_id >= qty || elem_id < 0 )
        return( FALSE );

    p_mesh      = MESH_P( analy );
    mtl_list    = p_mo_class->objects.elems->mat;
    material    = mtl_list[elem_id];
    hide_mtl    = p_mesh->hide_material;
    if ( hide_mtl[material] )
        return( FALSE );
    else
        return( TRUE );
}

/*****************************************************************
 * TAG( dump_selection_buff() )
 *
 * This function output an object selection buffer.
 */
void
dump_selection_buff( char *buff_name, unsigned char *buff, int qty )
{
    int i=0;
    for ( i=0;
            i<qty;
            i++ )
    {
        if ( (Bool_type) buff[i] )
            printf("\nBuffer %s[%d] = True\n", buff_name, i );
    }
}

/*****************************************************************
 * TAG( update_file_path() )
 *
 * This function will sub path variables of the form $pathn with
 * the true path.
 */
void
update_file_path( Analysis *analy, char *filename, char *filename_with_path )
{
    int i=0;
    Bool_type pathSet=FALSE;
    char pathvar[16];
    char *p_repl, *p_str;

    if ( strstr( filename, "$path" ) )
        pathSet = TRUE;

    if ( analy->paths_set[0] && !pathSet )   /* Append first path to filename */
    {
        strcpy( filename_with_path, analy->paths[0] );
        strcat( filename_with_path, "/" );
        strcat( filename_with_path, filename );
        return;
    }

    strcpy( filename_with_path, filename );

    for ( i=0;
            i<MAX_PATHS;
            i++ )
    {
        if ( analy->paths_set[i] && analy->paths[i] )
        {
            sprintf( pathvar, "$path%d", i+1 );
            p_str = strstr( filename, pathvar );
            if ( p_str )
            {
                p_repl = replace_string( filename_with_path, pathvar, analy->paths[i] );
                strcpy( filename_with_path, p_repl );
            }
        }
    }
}

/*****************************************************************
 * TAG( get_class_select_index )
 *
 * Returns index to by-class selection data
 */
int
get_class_select_index( Analysis *analy, char *class_name )
{
    Mesh_data *p_md;
    int class_index=0;
    int i=0;
    char class_name_upper[256], short_name_upper[256];

    string_to_upper( class_name, class_name_upper ); /* Make case insensitive */


    p_md = MESH_P( analy );
    for ( i=0;
            i<p_md->qty_class_selections;
            i++ )
    {
        string_to_upper( p_md->by_class_select[class_index].p_class->short_name, short_name_upper ); /* Make case insensitive */

        if (!strcmp( class_name_upper, short_name_upper ))
            return( class_index );
        else class_index++;
    }
    return (-1);
}

int
setElementSet_integration_point(ElementSet* element_set,int selection, 
                                int temp_value,int pt)
{
     /* The return_status convention is as follows:
 *     Value         Meaning
 *     21------------This label array was marked as invalid and will not be used
 *     0-------------The ipt was found in the labels array and was selected.
 *     22------------The ipt had a value less than the lowest value in the labels array.
 *                   So the lowest value was set.
 *     1-------------The ipt is between two values in the labels array and its distance from
 *                   the lower value is less than or equal to the distance from the higher
 *                   value. The value set is the lower value.
 *     2-------------The ipt is between two values in the labels array and its distance from
 *                   the lower value is greater than the distance from the higher value. The 
 *                   value set is the higher value.
 *     3-------------The ipt had a value higher than the highest value in the labels array. 
 *                   The value set is the highest value in the labels array.
 *  
 *            THE FOLLOWING VALUES ARE SET IF USING THE TOKEN AND ITS VALUE IS "inner"
 *     0-------------The lowest value in the labels array is equal to 1 The value set is 1
 *     10------------The lowest value in the labels array > 1 The value set is the lowest value
 *                   in the labels array.
 *            THE FOLLOWING VALUES ARE SET IF USING THE TOKEN AND ITS VALUE IS "middle"
 *                   The middle point is calculated and the function calls itself recursively
 *                   with that value.  The return values are as indicated above using a selected
 *                   integration point rather than the token
 *
 *            THE FOLLOWING VALUES ARE SET IT USING THE TOKEN AND ITS VALUE IS "outer"
 *     0-------------The labels->labels[index][size - 1] is equal to labels->labels[index][size] 
 *                   The value set is the value in labels->labels[index][size - 1]
 *     20------------The labels->labels[index][size - 1] < labels->labels[index][size].  The
 *                   value set is the the value in labels->labels[index][size - 1] 
 */
    int i;
    int return_value=0;
    switch(selection)
    {
        case(0):
            element_set->current_index = 0;
            break;
        case(1):
            element_set->current_index = element_set->middle_index;
            break;
        case(2):
            element_set->current_index = element_set->size-1;
            break;
        case(3):
            // It is not inner, middle, or outer
            if(pt < 1)
            {
				return_value = -1;
            }
            else if(pt < element_set->integration_points[0])
            {
                return_value = -1;
            	if(temp_value){
                    element_set->tempIndex = 0;
                }
                else{
                    element_set->current_index = 0;
                    element_set->tempIndex = -1; 
                }
            }
            else if(pt > element_set->integration_points[element_set->size-1])
            {
                return_value = -1;
            	if(temp_value){
                    element_set->tempIndex = element_set->size-1;
            	}
                else{
                    element_set->current_index = element_set->size-1;
                    element_set->tempIndex = -1;
    			}
            }
			else
            {
            	// We will need to search the integration point and find the closest
                for(i = 0; i < element_set->size; i++)
                {
					if(pt == element_set->integration_points[i]){
						if(temp_value){
							element_set->tempIndex = i;
						}
						else{
							element_set->current_index = i;
							element_set->tempIndex = -1; 
						}
                        break;
					}
                	else if(pt < element_set->integration_points[i])
                	{
						return_value = -1;
                    	if(pt-element_set->integration_points[i-1] < element_set->integration_points[i]-pt)
                    	{
                        	if(temp_value)
                        	{
                            	element_set->tempIndex = i-1;
                        	}else
                        	{
                            	element_set->current_index = i-1;
                            	element_set->tempIndex = -1;
                        	}    
                    	}else
                    	{
                        	if(temp_value)
                        	{
                            	element_set->tempIndex = i;
                        	}else
                        	{
                            	element_set->current_index = i;
                            	element_set->tempIndex = -1;
                        	}
                    	}
                    	break;
                	}
                }
            }
            break;
        default:
            //We should not get here as the only choices are 0-3
            //We opt to doing nothing
            break;
    }
    
    
    return return_value;
}
/*************************************************************************
 * TAG( select_integration_pts )
 *  selects/deselects integrations points specified in the command line
 *  returns TRUE or FALSE depending on the command line syntax
 ************************************************************************/
int select_integration_pts(char tokens[MAXTOKENS][TOKENLENGTH], int token_cnt, 
                           Analysis * analy)
{
    //char tokens[MAXTOKENS][TOKENLENGTH];
    char suffix[124];
    char showcmd[124];
    char update_msg[80];
    Htable_entry *tempEnt;
    int i, j, k, status;
    int pt, index;
    int size;
    int selection;
    int usetoken = 0;
    Bool_type found = FALSE;
    int mat_qty;
    int mat_min;
    int mat_max;
    int return_status;
    int message_map[23];
    intPtMessages * message = NULL;
    intPtMessages * current_message = NULL;
    
    intPtMessages * p;
    intPtMessages * q;
    int *chosen_materials = NULL;
    ElementSet *element_set; 
    
    if (token_cnt <2)
    {
        return GRIZ_FAIL;
    }
    mat_qty = MESH(analy).material_qty;
    selection = -1;
    index = -1;
    if(!strcmp(tokens[1], "inner"))
    {
        selection = 0;
    } else if(!strcmp(tokens[1], "middle"))
    {
        selection = 1;
    } else if(!strcmp(tokens[1], "outer"))
    {
        selection = 2;
    }
              
    if(selection < 0 )
    {
        // We must have an integer value
        pt = atoi(tokens[1]);
        
        if(pt == 0)
        {
            popup_dialog(WARNING_POPUP, "Invalid command syntax for set_ipt.\n");
            return FALSE;
        } 
        selection = 3;   
    }
    
    if(token_cnt > 2)
    {
        char target[20];
        char *start = "IntLabel_es_";
        for(i=2; i<token_cnt;i++)
        {
            parse_mtl_range(tokens[i], mat_qty, &mat_min, &mat_max);
            for(j=mat_min;j<=mat_max;j++)
            {
                sprintf(target,"%s%d",start,j);
                tempEnt = NULL;
                status = htable_search( analy->Element_sets, target, FIND_ENTRY,&tempEnt);
                if(status == OK)
                {
                    element_set = (ElementSet*)tempEnt->data;
                    return_status = setElementSet_integration_point(element_set, selection, 0, pt);
                    if(return_status == OK)
                    {
                        pt = element_set->integration_points[element_set->current_index];
                        sprintf(update_msg, "\n  Material %d integration point set to: %d\n", j, pt);
                        wrt_text(update_msg);
                    }
                    else
                    {
                        if(message == NULL)
                        {
                            current_message = message = (intPtMessages *) malloc(1*sizeof(intPtMessages));
                            if(message == NULL)
                            {
                                popup_dialog(WARNING_POPUP, 
                                             "Out of memory in function select_integration_pts. exiting\n");
                                parse_command("quit", analy);
                            }
                            message->next = NULL;
                            message->prev = NULL;
                         }else
                         {
                             current_message->next = (intPtMessages *) malloc(1*sizeof(intPtMessages));
                             current_message->next->next = NULL;
                             current_message->next->prev = current_message;
                             current_message = current_message->next;
                         }
                                                        
                         sprintf(current_message->messages, "  %d                    %d                       %d      \n", 
                                 element_set->material_number, 
                                 pt, 
                                 element_set->integration_points[element_set->current_index]);
                        
                    }
                }
            }
        } 
    }
    else
    {
        for(i = 0; i < analy->es_cnt; i++)
        {
            tempEnt = NULL;
            status = htable_search( analy->Element_sets, analy->Element_set_names[i], FIND_ENTRY,&tempEnt);
            if(status == OK)
            {
                element_set = (ElementSet*)tempEnt->data;
                return_status = setElementSet_integration_point(element_set, selection, 0, pt);
                if(return_status == OK)
                {
                    pt = element_set->integration_points[element_set->current_index];
                    sprintf(update_msg, "\n  Material %d integration point set to: %d\n", element_set->material_number, pt);
                    wrt_text(update_msg);
                }
                else
                {
                    if(message == NULL)
                    {
                         current_message = message = (intPtMessages *) malloc(1*sizeof(intPtMessages));
                         if(message == NULL)
                         {
                             popup_dialog(WARNING_POPUP, 
                                          "Out of memory in function select_integration_pts. exiting\n");
                             parse_command("quit", analy);
                         }
                         message->next = NULL;
                         message->prev = NULL;
                     }else
                     {
                         current_message->next = (intPtMessages *) malloc(1*sizeof(intPtMessages));
                         current_message->next->next = NULL;
                         current_message->next->prev = current_message;
                         current_message = current_message->next;
                     }
                                                        
                     sprintf(current_message->messages, "  %d                    %d                       %d      \n", 
                             element_set->material_number, 
                             pt, 
                             element_set->integration_points[element_set->current_index]);
                        
                }
            }
        }
    }
    
    i=0;
    if( message != NULL)
    {
        wrt_text("\n\nWARNING: The desired integration point for element set/material\n is not available for:\n\n");
        wrt_text("Material            Desired Int             Selected Int.\n");
        wrt_text("Element Set          Point                   Point    \n");

        for(p = message; p != NULL; p = p->next)
            wrt_text(p->messages);
        
        p = message;
        while(p != NULL){
            q = p;
            p = p->next;
            free(q);
        }
        p = NULL; q = NULL;
            
        popup_dialog(WARNING_POPUP, "The exact Integration point(s) are unavailable for select materials/element sets. See feedback window.\n");
    }
    
    if(analy->cur_result != NULL)
    {
        sprintf(showcmd, "show %s", analy->cur_result->name);
        parse_command(showcmd, analy);
    }
    return TRUE;
}


/********************************************************
 * TAG( show_ipt_avail )
    }
 * prints the integration points availableon the 
 * feedback window 
 *******************************************************/

void show_ipt_avail(Analysis * analy)
{
    int i, j;
    int index, mat,status;
    ElementSet *element_set;
    wrt_text("\n\nAvailable Integration Points in this plot file are as follows:\n");
    
    Htable_entry *tempEnt;
   Htable_entry *matEntry;
    char label[34];
    char intpts[40];
    char intpt[4];
   char mat_num[5];
    int snum;
   intpts[0] = '\0';
    char *line = "--------------------------------------------------------------------";
    wrt_text(" %-34s| %-10s | %-30s\n", "Material", "Int. Point", "Int. Point");
    wrt_text(" %-34s| %-10s | %-30s\n", "", "Selected", "Available");
   wrt_text("%-62s\n", line);
    
   for(i=0; i<analy->es_cnt;i++)
   {
       status = htable_search(analy->Element_sets,analy->Element_set_names[i],FIND_ENTRY,&tempEnt);
       if(status != OK)
       {
           wrt_text("%s %d\n" , "Error loading Integration points for element set ", analy->Element_set_names[i] );
           return;
       }
       
       element_set = (ElementSet*)tempEnt->data;
       
       index = element_set->current_index;
       sprintf(mat_num,"%d", element_set->material_number);
       status = htable_search(analy->mat_labels_reversed,mat_num,FIND_ENTRY,&matEntry);
       sprintf(label,"%s" , (char*)matEntry->data);
       for(j=0;j<element_set->size;j++)
       {
           if(j<element_set->size-1)
           {
               sprintf(intpt,"%d, ",element_set->integration_points[j]);
           }else
           {
               sprintf(intpt,"%d ",element_set->integration_points[j]);
           }
           strcat(intpts, intpt);
           
       }
       wrt_text(" %-34s|    %-6d  | %-30s\n", label, element_set->integration_points[index], intpts);
       intpts[0] = '\0';
   } 
    
    return;
}

void restore_color(Analysis * analy, Bool_type use_default){

    Bool_type continueRestore = False;
    float** ambient;
    float** diffuse;
    float** specular;
    float** emission;
    float* shininess;

	if(use_default){
		if(analy->defaultColorActive){
			ambient = analy->default_ambient;
			diffuse = analy->default_diffuse;
			specular = analy->default_specular;
			emission = analy->default_emission;
			shininess = analy->default_shininess;
			continueRestore = True;
		}
	}
	else{
		if(analy->lastColorActive){
			ambient = analy->last_ambient;
			diffuse = analy->last_diffuse;
			specular = analy->last_specular;
			emission = analy->last_emission;
			shininess = analy->last_shininess;
			continueRestore = True;
		}
	}

	if(continueRestore){
		int colorPos = 0;
		for(colorPos = 0; colorPos < analy->max_mesh_mat_qty; colorPos++){
			//default colors
			v_win->mesh_materials.ambient[colorPos][0] = ambient[colorPos][0];
			v_win->mesh_materials.ambient[colorPos][1] = ambient[colorPos][1];
			v_win->mesh_materials.ambient[colorPos][2] = ambient[colorPos][2];
			glMaterialfv( GL_FRONT_AND_BACK, GL_AMBIENT, v_win->mesh_materials.ambient[colorPos] );
			v_win->mesh_materials.diffuse[colorPos][0] = diffuse[colorPos][0];
			v_win->mesh_materials.diffuse[colorPos][1] = diffuse[colorPos][1];
			v_win->mesh_materials.diffuse[colorPos][2] = diffuse[colorPos][2];
			v_win->mesh_materials.diffuse[colorPos][3] = diffuse[colorPos][3];
			glMaterialfv( GL_FRONT_AND_BACK, GL_DIFFUSE, v_win->mesh_materials.diffuse[colorPos] );
			v_win->mesh_materials.specular[colorPos][0] = specular[colorPos][0];
			v_win->mesh_materials.specular[colorPos][1] = specular[colorPos][1];
			v_win->mesh_materials.specular[colorPos][2] = specular[colorPos][2];
			glMaterialfv( GL_FRONT_AND_BACK, GL_SPECULAR, v_win->mesh_materials.specular[colorPos] );
			v_win->mesh_materials.emission[colorPos][0] = emission[colorPos][0];
			v_win->mesh_materials.emission[colorPos][1] = emission[colorPos][1];
			v_win->mesh_materials.emission[colorPos][2] = emission[colorPos][2];
			glMaterialfv( GL_FRONT_AND_BACK, GL_EMISSION, v_win->mesh_materials.emission[colorPos] );
			v_win->mesh_materials.shininess[colorPos] = shininess[colorPos];
			glMaterialf( GL_FRONT_AND_BACK, GL_SHININESS, v_win->mesh_materials.shininess[colorPos] );
		}
	}
}

void backup_current_colors(Analysis *analy, Bool_type use_default){

	float** ambient;
	float** diffuse;
	float** specular;
	float** emission;
	float* shininess;

	if(use_default){
		analy->defaultColorActive = True;
		ambient = analy->default_ambient;
		diffuse = analy->default_diffuse;
		specular = analy->default_specular;
		emission = analy->default_emission;
		shininess = analy->default_shininess;
	}
	else{
		analy->lastColorActive = True;
		ambient = analy->last_ambient;
		diffuse = analy->last_diffuse;
		specular = analy->last_specular;
		emission = analy->last_emission;
		shininess = analy->last_shininess;
	}

	int colorPos = 0;
	for(colorPos = 0; colorPos < analy->max_mesh_mat_qty; colorPos++){
		//default colors
		ambient[colorPos][0] = v_win->mesh_materials.ambient[colorPos][0];
		ambient[colorPos][1] = v_win->mesh_materials.ambient[colorPos][1];
		ambient[colorPos][2] = v_win->mesh_materials.ambient[colorPos][2];
		diffuse[colorPos][0] = v_win->mesh_materials.diffuse[colorPos][0];
		diffuse[colorPos][1] = v_win->mesh_materials.diffuse[colorPos][1];
		diffuse[colorPos][2] = v_win->mesh_materials.diffuse[colorPos][2];
		diffuse[colorPos][3] = v_win->mesh_materials.diffuse[colorPos][3];
		specular[colorPos][0] = v_win->mesh_materials.specular[colorPos][0];
		specular[colorPos][1] = v_win->mesh_materials.specular[colorPos][1];
		specular[colorPos][2] = v_win->mesh_materials.specular[colorPos][2];
		emission[colorPos][0] = v_win->mesh_materials.emission[colorPos][0];
		emission[colorPos][1] = v_win->mesh_materials.emission[colorPos][1];
		emission[colorPos][2] = v_win->mesh_materials.emission[colorPos][2];
		shininess[colorPos] = v_win->mesh_materials.shininess[colorPos];
	}
}


///************************************************************
// * TAG( restore_colors )
// *
// * restore a material's rendering properties.
// */
//void restore_colors(Analysis* analy, Bool_type lastColors){
//
//	Bool_type continueRestore = FALSE;
//    float** ambient;
//    float** diffuse;
//    float** specular;
//    float** emission;
//    float* shininess;
//
//	if(lastColors){
//		if(analy->lastColorActive){
//			ambient = analy->last_ambient;
//			diffuse = analy->last_diffuse;
//			specular = analy->last_specular;
//			emission = analy->last_emission;
//			shininess = analy->last_shininess;
//			continueRestore = TRUE;
//		}
//	}
//	else{
//		if(analy->defaultColorActive){
//			ambient = analy->default_ambient;
//			diffuse = analy->default_diffuse;
//			specular = analy->default_specular;
//			emission = analy->default_emission;
//			shininess = analy->default_shininess;
//			continueRestore = TRUE;
//		}
//	}
//	if(continueRestore){
//		int mtl = 0;
//		for(mtl = 1; mtl <= analy->max_mesh_mat_qty; mtl++){
//			char cmd_str[100];
//			//ambient
//			sprintf(cmd_str, "mat %s amb %f %f %f", mtl, ambient[mtl][0], ambient[mtl][1], ambient[mtl][2]);
//			parse_single_command(cmd_str, analy);
//			//diffuse
//			sprintf(cmd_str, "mat %s dif %f %f %f", mtl, diffuse[mtl][0], diffuse[mtl][1], diffuse[mtl][2]);
//			parse_single_command(cmd_str, analy);
//			//specular
//			sprintf(cmd_str, "mat %s spec %f %f %f", mtl, specular[mtl][0], specular[mtl][1], specular[mtl][2]);
//			parse_single_command(cmd_str, analy);
//			//emission
//			sprintf(cmd_str, "mat %s emis %f %f %f", mtl, emission[mtl][0], emission[mtl][1], emission[mtl][2]);
//			parse_single_command(cmd_str, analy);
//			//shininess
//			sprintf(cmd_str, "mat %s shine %f", mtl, shininess);
//			parse_single_command(cmd_str, analy);
//			//alpha
//			sprintf(cmd_str, "mat %s alpha %f", mtl, diffuse[mtl][3]);
//			parse_single_command(cmd_str, analy);
//		}
//	}
//
//}

