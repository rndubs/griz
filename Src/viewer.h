/* $Id$ */
/*
 * viewer.h - Definitions for MDG mesh viewer.
 *
 *      Donald J. Dovey
 *      Lawrence Livermore National Laboratory
 *      Jan 2, 1992
 *
 ************************************************************************
 * Modifications:
 *
 *  I. R. Corey - Sept 15, 2004: Added new option "damage_hide" which
 *  controls if damaged elements are displayed.
 *
 *  I. R. Corey - Dec 22, 2004: Added new option "free_nodes" which
 *  controls the display of free_node data. See SCR#: 286
 *
 *  I. R. Corey - Jan 24, 2005: Added a new free_node option to disable
 *                mass scaling of the nodes.
 *
 *  I. R. Corey - Feb 10, 2005: Added a new free_node option to enable
 *                volume disable scaling of the nodes.
 *
 *  I. R. Corey - Mar 22, 2005: Added a new stateDB flag so we can
 *                test for a state or time history database. Also
 *                added a new function for loading nodal positions
 *                for time history databases.
 *
 *  I. R. Corey - May 19, 2005: Added new option to allow for selecting
 *                a alpha version of the code. This option (-alpha) is
 *                also parsed by the griz4s shell script.
 *                See SCR #328.
 *
 *  I. R. Corey - May 23, 2005: Added variable to identify results that
 *                must be recalculated.
 *                See SRC#: 316
 *
 *  I. R. Corey - January 24, 2006: Added variable to identify results that
 *                must be recalculated.
 *                See SRC#: 316
 *
 *  I. R. Corey - March 14, 2007: Added struct and functions to support
 *                writing and reading session data.
 *                See SRC#: 439
 *
 *  I. R. Corey - August 14, 2007: Added an env flag for enabling TI
 *                data plotting.
 *                See SRC#: 480
 *
 *  I. R. Corey - Aug 09, 2007:	Added date/time string to render window.
 *                See SRC#478.
 *
 *  I. R. Corey - Aug 15, 2007:	Added menus for displaying TI results.
 *                See SRC#480.
 *
 *  I. R. Corey - Nov 09, 2009: Added enhancements to better support
 *                running multiple sessions of Griz.
 *                See SRC#638.
 *
 *  I. R. Corey - April 18, 2012: Added ehnancement to enforce checking
 *                for enabled materials for selections.
 *
 *  I. R. Corey - May 2nd, 2012: Added path to top of window panes.
 *                See TeamForge#17900
 *
 *************************************************************************
 */

#ifndef VIEWER_H
#define VIEWER_H

#include <time.h>

#include <limits.h>

#include <sys/types.h>

#include "draw.h"
#include "io_wrap.h"
#include "misc.h"
#include "list.h"
#include "geometric.h"
#include "results.h"
#include "mesh.h"
#include "gahl.h"


#define MAXPATHLENGTH 1024
#define MAXFILENAMELENGTH 512

#ifndef MAXINT
#define MAXINT 2147483647
#endif
#ifndef MININT
#define MININT -MAXINT
#endif
#ifndef MAXFLOAT
#define MAXFLOAT ((float)3.40282347e+38)
#endif
#ifndef MINFLOAT
#define MINFLOAT -MAXFLOAT
#endif

/* Define usage of new Mili Library - July 5, 2011 */
#define NEWMILI 1

/* Dimensions for default and video rendering window sizes */

#ifdef SERIAL_BATCH
/*
 * Make certain DEFAULT_WIDTH and DEFAULT_HEIGHT are less than or equal
 * to MAX_WIDTH and MAX_HEIGHT contained in Mesa Library /src/config.h
 */

#define DEFAULT_WIDTH (1200)
#define DEFAULT_HEIGHT (1200)

#else

#define DEFAULT_WIDTH (600)
#define DEFAULT_HEIGHT (600)

#endif /* SERIAL_BATCH */

#define VIDEO_WIDTH (720)
#define VIDEO_HEIGHT (486)

/* Default number of significant figures in floating point numbers. */
#define DEFAULT_FLOAT_FRACTION_SIZE (2)

/* Default number of significant figures in times. */
#define DEFAULT_TIME_FRACTION_SIZE (5)
#define DEFAULT_TH_TIME_FRACTION_SIZE (6)

/* Number of materials with settable properties. */
#define MAX_MATERIALS 300

/* Default depth-buffer bias for rendering lines in front of polygons. */
#define DFLT_ZBIAS .005

/* Quantity of temporary result arrays to allocate. */
#define TEMP_RESULT_ARRAY_QTY 7

/* A nominal return value. */
#ifndef OK
#define OK 0
#endif

#ifndef NOT_OK
#define NOT_OK 0
#endif

/* A failure return value. */
#ifndef GRIZ_FAIL
#define GRIZ_FAIL 1
#endif

#define MAXHIST 1000

#define MAX_PATHS 10

/* Macro complement to get_max_state() function. */
#define GET_MIN_STATE( a ) ( (a)->limit_min_state ? (a)->min_state : 0 )

#ifndef HAVE_WEDGE_PYRAMID
#define HAVE_WEDGE_PYRAMID
#endif

#define ORIG_RESULTS 20
/* OpenGL windows. */
typedef enum
{
    MESH_VIEW,
    SWATCH,
    NO_OGL_WIN /* Always last! */
} OpenGL_win;


#define MAXINFOMSG 20

/*****************************************************************
 * TAG( Result_table_type )
 *
 * Enumerations of possible result types.
 */
typedef enum
{
    DERIVED,
    PRIMAL,
    ALL
} Result_table_type;


/*****************************************************************
 * TAG( RGB_raster_obj )
 *
 * An in-memory RGB or RGBA image raster with associated information.
 */
typedef struct _rgb_raster_obj
{
    int img_width;
    int img_height;
    unsigned char *raster;
    Bool_type alpha;
} RGB_raster_obj;

/*****************************************************************
 * TAG( Trace_segment_obj )
 *
 * Information about a particle subtrace which exists within one
 * material.
 */
typedef struct _Trace_segment
{
    struct _Trace_segment *next;
    struct _Trace_segment *prev;
    int material;
    int last_pt_idx;
} Trace_segment_obj;


/*****************************************************************
 * TAG( Trace_pt_def_obj )
 *
 * Start point definition for a particle trace.
 */
typedef struct _Trace_pt_def_obj
{
    struct _Trace_pt_def_obj *next;
    struct _Trace_pt_def_obj *prev;
    float pt[3];
    int elem;
    int material;
    float color[3];
    float xi[4];
    MO_class_data *mo_class;
} Trace_pt_def_obj;


/*****************************************************************
 * TAG( Trace_pt_obj )
 *
 * Start point for a particle trace.
 */
typedef struct _Trace_pt_obj
{
    struct _Trace_pt_obj *next;
    struct _Trace_pt_obj *prev;
    float pt[3];
    int elnum;
    float xi[4];
    float vec[3];
    MO_class_data *mo_class;
    Bool_type in_grid;
    int cnt;
    float *pts;
    float *time;
    float color[3];
    int mtl_num;
    int last_elem;
    Trace_segment_obj *mtl_segments;
} Trace_pt_obj;


/*****************************************************************
 * TAG( Plane_obj )
 *
 * A single plane.
 */
typedef struct _Plane_obj
{
    struct _Plane_obj *next;
    struct _Plane_obj *prev;
    float pt[3];
    float norm[3];
} Plane_obj;


/*****************************************************************
 * TAG( Refl_plane_obj )
 *
 * A reflection plane.
 */
typedef struct _Refl_plane_obj
{
    struct _Refl_plane_obj *next;
    struct _Refl_plane_obj *prev;
    float pt[3];
    float norm[3];
    Transf_mat pt_transf;
    Transf_mat norm_transf;
} Refl_plane_obj;


/*****************************************************************
 * TAG( Damage_vals )
 *
 * Stores parameters for damage result calculation.
 */
typedef struct _Damage_vals
{
    char vel_dir[2];
    float cut_off[3];
} Damage_vals;

/*****************************************************************
 * TAG( Result_spec )
 *
 * Stores information to unambiguously identify a result.
 */
typedef struct _Result_spec
{
    char name[M_MAX_NAME_LEN];
    Strain_type strain_variety;
    Ref_surf_type ref_surf;
    Ref_frame_type ref_frame;
    int ref_state;
    Result_modifier_flags use_flags;
} Result_spec;


/*****************************************************************
 * TAG( Result )
 *
 * Stores all information needed to manage and render a result.
 * Some data is state dependent because of potential state record
 * format changes.
 */
typedef struct _result
{
    struct _result *next;
    struct _result *prev;
    char root[512];
    char name[M_MAX_NAME_LEN];
    char title[64];
    char original_name[M_MAX_NAME_LEN];
    char **original_names;
    int qty;
    int *subrecs;
    int *superclasses;
    Bool_type *indirect_flags;
    void (** result_funcs)();
    int mesh_id;
    int srec_id;
    char ***primals;
    Result_origin_flags origin;
    Result_spec modifiers;
    Bool_type single_valued;
    void *original_result;
    int reference_count;

    /* IRC: Added 05/23/05 */
    Bool_type must_recompute;

    /* IRC: Added 08/30/05 */
    Bool_type hide_in_menu;
} Result;


/*****************************************************************
 * TAG( Minmax_obj )
 *
 * Stores the global min/max for a specific result variable.
 */
typedef struct _Minmax_obj
{
    struct _Minmax_obj *next;
    struct _Minmax_obj *prev;
    Result_spec result;
    float interpolated_minmax[2];     /* Nodal result (original or interpolated). */
    float object_minmax[2];           /* Element result. */
    int object_id[2];
    char *class_long_name[2];
    int  sclass[2];
} Minmax_obj;


/*****************************************************************
 * TAG( Plot_operation_type )
 *
 * Enumeration of "operation plot" operators.
 */
typedef enum
{
    OP_NULL = 0,
    OP_DIFFERENCE,
    OP_SUM,
    OP_PRODUCT,
    OP_QUOTIENT
} Plot_operation_type;

/*****************************************************************
 * TAG( Analysis_type )
 *
 * Enumeration of analysis types.
 */
typedef enum
{
    TIME,
    MODAL
} Analysis_type;

/*****************************************************************
 * TAG( Time_series_obj )
 *
 * Time series result data and associated validating parameters.
 */
typedef struct _time_series_obj
{
    struct _time_series_obj *next;
    struct _time_series_obj *prev;
    int min_eval_st; /* 0... */
    int max_eval_st; /* 0... */
    int qty_blocks;
    Int_2tuple *state_blocks;
    int cur_block;
    float *data;
    int qty_states;
    List_head merged_formats;
    List_head series_map;
    int ident;
    MO_class_data *mo_class;
    Plot_operation_type op_type;
    struct _time_series_obj *operand1;
    Bool_type update1;
    struct _time_series_obj *operand2;
    Bool_type update2;
    Result *result;
    float min_val;
    int min_val_state; /* 0... */
    float max_val;
    int max_val_state; /* 0... */
    int reference_count;
} Time_series_obj;


/*****************************************************************
 * TAG( Plot_obj )
 *
 * Abscissa and ordinate references for a 2D plot.
 */
typedef struct _plot_obj
{
    struct _plot_obj *next;
    struct _plot_obj *prev;
    Time_series_obj *abscissa;
    Time_series_obj *ordinate;
    int index;
} Plot_obj;


/*****************************************************************
 * TAG( Specified_obj )
 *
 * Specified object identification data.
 */
typedef struct _specified_obj
{
    struct _specified_obj *next;
    struct _specified_obj *prev;
    int ident;
    int label;
    MO_class_data *mo_class;
} Specified_obj;


/*****************************************************************
 * TAG( Series_ref_obj )
 *
 * Storage for a reference to a Time_series_obj in a linked-list.
 */
typedef struct _series_ref_obj
{
    struct _series_ref_obj *next;
    struct _series_ref_obj *prev;
    Time_series_obj *series;
    Bool_type active;
} Series_ref_obj;


/*****************************************************************
 * TAG( Result_mo_list_obj )
 *
 * Linked-list node for a Result reference with a list of
 * references to supported objects.
 */
typedef struct _result_mo_list_obj
{
    struct _result_mo_list_obj *next;
    struct _result_mo_list_obj *prev;
    Result *result;
    Series_ref_obj *mo_list;
    Bool_type active;
} Result_mo_list_obj;


/*****************************************************************
 * TAG( ElementSet )
 *
 * Information about a Element Set.
 */
typedef struct _ElementSet
{
  int size;                 /* Size of the integration_points in the array */
  int * integration_points;  /* The actual integration point put out by the code the int at
                               size is the simulation actual integration points processed*/
  int ipt_count;                // This is the size of the array.
  int current_index;        /* The selected integration point by the user. Default is zero? */
  int tempIndex;
  int middle_index;
  int material_number;
} ElementSet;


/*****************************************************************
 * TAG( Subrec_obj )
 *
 * Information about a subrecord of a state record format.
 */
typedef struct _subrec_obj
{
    int *object_ids; /* NULL if Subrecord binds class completely and in order */
    MO_class_data *p_object_class;
    int sand;
    int *referenced_nodes;
    int ref_node_qty;
    Subrecord subrec;
    ElementSet *element_set;

    /*
     * Handle for temporary storage of references to results
     * supported by this subrecord  and associated
     * lists of supported objects bound to this subrecord.
     */
    Result_mo_list_obj *gather_tree;
} Subrec_obj;


/*****************************************************************
 * TAG( State_rec_obj )
 *
 * Struct to hold Subrec_obj array for a state record format and
 * other useful information global to the state record format.
 */
typedef struct _state_rec_obj
{
    int qty;
    Subrec_obj *subrecs;
    int node_pos_subrec;
    int node_vel_subrec;
    int particle_pos_subrec;
    int series_qty;
} State_rec_obj;


/*****************************************************************
 * TAG( Classed_list )
 *
 * Head of a list of objects associated with a single class.
 */
typedef struct _classed_list
{
    struct _classed_list *next;
    struct _classed_list *prev;
    MO_class_data *mo_class;
    void *list;
} Classed_list;


/*****************************************************************
 * TAG( Triangle_poly )
 *
 * Triangular polygon.
 */
typedef struct _Triangle_poly
{
    struct _Triangle_poly *next;
    struct _Triangle_poly *prev;
    float vtx[3][3];
    float result[3];
    float norm[3];
    int mat;
    int elem;
} Triangle_poly;


/*****************************************************************
 * TAG( Surf_poly )
 *
 * Surface polygon.
 */
typedef struct _Surf_poly
{
    struct _Surf_poly *next;
    struct _Surf_poly *prev;
    int cnt;
    float vtx[4][3];
    float norm[4][3];
    int mat;
} Surf_poly;


/*****************************************************************
 * TAG( Ref_poly )
 *
 * Reference polygon.
 */
typedef struct _Ref_poly
{
    struct _Ref_poly *next;
    struct _Ref_poly *prev;
    int nodes[4];
    int mat;
} Ref_poly;


/*****************************************************************
 * TAG( Contour_obj )
 *
 * Holds a contour segment.
 */
typedef struct _Contour_obj
{
    struct _Contour_obj *next;
    struct _Contour_obj *prev;
    int cnt;
    float *pts;
    float contour_value;
    float start[2];
    float end[2];
} Contour_obj;


/*****************************************************************
 * TAG( MO_list_token_type )
 *
 * Types of tokens in a mesh object specification list.
 */
typedef enum
{
    NUMERIC,
    MESH_OBJ_CLASS,
    RANGE_SEPARATOR,
    COMPOUND_TOKEN,
    OTHER_TOKEN
} MO_list_token_type;


/*****************************************************************
 * TAG( Database_type )
 *
 * Enumerations of database types Griz can handle.
 */
typedef enum
{
    MILI,
    TAURUS,
    EXODUS
} Database_type_griz;


/*****************************************************************
 * TAG( Mesh_view_mode_type )
 *
 * Enumeration of mesh view (as opposed to plot) rendering styles.
 */
typedef enum
{
    RENDER_FILLED,
    RENDER_HIDDEN,
    RENDER_WIREFRAME,
    RENDER_WIREFRAMETRANS,
    RENDER_GS,
    RENDER_POINT_CLOUD,
    RENDER_NONE
} Mesh_view_mode_type;


/*****************************************************************
 * TAG( Interp_mode_type )
 *
 * Values for the interpolation flag.
 */
typedef enum
{
    NO_INTERP,
    REG_INTERP,
    GOOD_INTERP
} Interp_mode_type;


/*****************************************************************
 * TAG( Mouse_mode_type )
 *
 * Various modes for mouse input.
 */
typedef enum
{
    MOUSE_HILITE,
    MOUSE_SELECT
} Mouse_mode_type;


/*****************************************************************
 * TAG( Trans_type )
 *
 * Various modes for the translation widgets.
 */
typedef enum
{
    TRANSLATE_MESH,
    TRANSLATE_LIGHT,
    TRANSLATE_FROM,
    TRANSLATE_AT
} Trans_type;


/*****************************************************************
 * TAG( Filter_type )
 *
 * Filter types for time-history plot smoothing.
 */
typedef enum
{
    BOX_FILTER,
    TRIANGLE_FILTER,
    SYNC_FILTER
} Filter_type;


/*****************************************************************
 * TAG( Exploded_view_type )
 *
 * Types of material exploded views.
 */
typedef enum
{
    SPHERICAL,
    CYLINDRICAL,
    AXIAL
} Exploded_view_type;


/*****************************************************************
 * TAG( Center_view_sel_type )
 *
 * Ways of selecting a center for centering the view.
 */
typedef enum
{
    NO_CENTER = 0,
    HILITE,
    NODE,
    POINT,
    SELECT
} Center_view_sel_type;


/*****************************************************************
 * TAG( Material_property_type )
 *
 * Material manager material properties.
 */

typedef enum
{
    AMBIENT,
    DIFFUSE,
    SPECULAR,
    EMISSIVE,
    SHININESS,
    MTL_PROP_QTY /* Always last! */
} Material_property_type;

/*****************************************************************
 * TAG( Util_panel_button_type )
 *
 * Function buttons in the Utility Panel.
 * Defined here for access by interpret.c as well as gui.c.
 */
typedef enum
{
    STEP_FIRST,
    STEP_UP,
    STEP_DOWN,
    STEP_LAST,
    STRIDE_EDIT,
    STRIDE_INCREMENT,
    STRIDE_DECREMENT,
    VIEW_SOLID,
    VIEW_SOLID_MESH,
    VIEW_EDGES,
    VIEW_WIREFRAME,
    VIEW_WIREFRAMETRANS,
    VIEW_GS,
    VIEW_POINT_CLOUD,
    VIEW_NONE,
    PICK_MODE_SELECT,
    PICK_MODE_HILITE,
    BTN1_PICK,
    BTN2_PICK,
    BTN3_PICK,
    CLEAN_SELECT,
    CLEAN_HILITE,
    CLEAN_NEARFAR,
    BTN_CM_PICK,
    UTIL_PANEL_BTN_QTY /* Always last! */
} Util_panel_button_type;


/*****************************************************************
 * TAG( colormap_type )
 *
 * List of available colormaps.
 */
typedef enum
{
    CM_INVERSE,
    CM_DEFAULT,
    CM_COOL,
    CM_GRAYSCALE,
    CM_INVERSE_GRAYSCALE,
    CM_HSV,
    CM_JET,
    CM_PRISM,
    CM_WINTER
} colormap_type;


/*****************************************************************
 * TAG( Cursor_type )
 *
 * Enumeration of non-default cursors that Griz uses.
 */
typedef enum
{
    CURSOR_WATCH,
    CURSOR_EXCHANGE,
    CURSOR_FLEUR,
    CURSOR_QTY
} Cursor_type;


/*****************************************************************
 * TAG( Glyph_alignment_type )
 *
 * Enumeration of alignment modes for time history plot glyphs.
 */
typedef enum
{
    GLYPH_ALIGN,
    GLYPH_STAGGERED
} Glyph_alignment_type;


/*****************************************************************
 * TAG( Render_mode_type )
 *
 * Enumeration of basic rendering modes.
 */
typedef enum
{
    RENDER_ANY, /* A non-exclusive placeholder - not to be used explicitly. */
    RENDER_MESH_VIEW,
    RENDER_PLOT
} Render_mode_type;


/*****************************************************************
 * TAG( Redraw_mode_type )
 *
 * Enumeration of redrawing possibilities.
 */
typedef enum
{
    NO_VISUAL_CHANGE = 0,
    BINDING_MESH_VISUAL,
    NONBINDING_MESH_VISUAL,
    BINDING_PLOT_VISUAL,
    NONBINDING_PLOT_VISUAL
} Redraw_mode_type;


/*****************************************************************
 * TAG( Autoimg_format_type )
 *
 * Enumeration of basic rendering modes.
 */
typedef enum
{
    IMAGE_FORMAT_RGB = 0,
    IMAGE_FORMAT_JPEG,
    IMAGE_FORMAT_PNG
} Autoimg_format_type;


/*****************************************************************
 * TAG( TI_Var )
 *
 *   Struct which contains all the info related to TI variables
 *   loaded from the Mili database.
 */
typedef struct _TI_Var
{
    char      *long_name, *short_name;
    int       superclass;
    Bool_type nodal;
    int       type;
    int       length;
} TI_Var;


/*****************************************************************
 * TAG( combined_names )
 *
 *   Struct which contains the result names making up the combined
 *   result.
 ****************************************************************/
typedef struct _combined_names
{
    struct _combined_names *next;
    struct _combined_names *prev;
    char es_result_names[64];
} combined_names;


typedef struct _intPtMessages
{
    struct _intPtMessages * next;
    struct _intPtMessages * prev;
    char messages[256];
} intPtMessages;


typedef struct _mmHisEnt
{
	Minmax_obj elem_global_mm;		   	/* For result on element. */
	float global_mm[2];            	/* For result at nodes. */
	int global_mm_sclass[2];
	int global_mm_nodes[2];        	/* For result at nodes. */
	char *global_mm_class_long_name[2];
}mmHistEnt;


/*****************************************************************
 * TAG( Analysis )
 *
 * Struct which contains all the info related to an analysis.
 */
typedef struct _Analysis
{
    struct _Analysis *next;
    struct _Analysis *prev;

    int (*db_open)();
    int (*db_close)();
    int (*db_get_geom)();
    int (*db_get_state)();
    int (*db_get_st_descriptors)();
    int (*db_set_results)();
    int (*db_get_subrec_def)();
    int (*db_cleanse_subrec)();
    int (*db_cleanse_state_var)();
    int (*db_get_results)();
    int (*db_get_title)();
    int (*db_get_dimension)();
    int (*db_query)();
    int (*db_set_buffer_qty)();

    void (*update_display)( struct _Analysis * );
    void (*display_mode_refresh)( struct _Analysis * );
    Bool_type (*check_mod_required)( struct _Analysis *, Result_modifier_type, int, int );

    Bool_type autoselect;
    void * original_results[ORIG_RESULTS];   /* for combined results need to hold the original results for each superclass */
    int db_ident;
    char root_name[MAXPATHLENGTH];
    char path_name[MAXPATHLENGTH];
    Bool_type path_found;

    char mili_version[100];
    char mili_arch[100];
    char mili_timestamp[100];
    char mili_host[64];
    char xmilics_version[100];
    char job_id[128];

    char title[G_MAX_STRING_LEN];
    Bool_type limit_max_state;
    int max_state;
    Bool_type limit_min_state;
    int min_state;
    int cur_state;
    int previous_state;
    float *state_times;
    int reference_state;

    float  *cur_ref_state_data;
    double *cur_ref_state_dataDp;

    int state_count;
    int cur_mesh_id;
    Mesh_data *mesh_table;
    int mesh_qty;
    int max_mesh_mat_qty;
    /* New Field */
    int surface_qty;
    int max_mesh_surf_qty;
    int qty_srec_fmts;
    State_rec_obj *srec_tree;
    Hash_table *st_var_table;
    Hash_table *primal_results;
    Hash_table *derived_results;
    Bool_type mat_labels_active;
    Hash_table *mat_names;
    Hash_table *mat_names_reversed;
    char **sorted_names;
    Hash_table *mat_nums;
    Hash_table *mat_nums_reversed;
    char **sorted_nums;
    Hash_table *mat_labels;
    Hash_table *mat_labels_reversed;
    char **sorted_labels;
    int maxLabelLength;
    int sorted_names_reversed;
    char **banned_names;
    int num_banned_names;
    char **conflict_messages;
    int num_messages;
    Hash_table *intPoints;
    Result_table_type result_source;
    Result_table_type prev_result_source;
    Result_table_type preferred_primal_source; /* For derived results, do we prefer primal primals or derived primals */
    Bool_type have_hex_strains;
    char **component_menu_specs;
    int component_spec_qty;
    State2 *state_p;
    int dimension;
    float buffer_init_val;

    Bool_type normals_constant;
    float *node_norm[3];
    Bool_type shared_faces;
    float bbox[2][3];
    int bbox_source_qty;
    MO_class_data **bbox_source;
    Bool_type vis_mat_bbox;
    Bool_type keep_max_bbox_extent;

    Bool_type limit_rotations;

    Bool_type show_background_image;
    RGB_raster_obj *background_image;

    Render_mode_type render_mode;
    Bool_type refresh;
    Bool_type normals_smooth;
    Bool_type hex_overlap;
    Mesh_view_mode_type mesh_view_mode;
    Mesh_view_mode_type last_mesh_view_mode;
    Interp_mode_type interp_mode;
    Bool_type show_bbox;
    Bool_type show_coord;
    Bool_type show_time;
    Bool_type show_title, show_title_path;
    Bool_type show_safe;
    Bool_type show_colormap;
    Bool_type show_colorscale;
    Bool_type show_minmax;
    Bool_type show_scale;
    Bool_type show_datetime;
    Bool_type show_deleted_elements, show_only_deleted_elements;
    Bool_type show_tinfo;
    Bool_type show_master, show_slave;
    Bool_type show_interp;
    Bool_type use_snap;

    Bool_type use_colormap_pos;
    float hidden_line_width;
    int float_frac_size;
    int time_frac_size;
    Bool_type auto_frac_size;
    float colormap_pos[4];
    Mouse_mode_type mouse_mode;
    MO_class_data *pick_class;
    MO_class_data *hilite_class;
    int hilite_label;
    int hilite_num;
    int hilite_ml_node; /* The node reference for a Meshless object */
    Center_view_sel_type center_view;
    int center_node;
    float view_center[3];
    Specified_obj *selected_objects;

    Refl_plane_obj *refl_planes;
    Bool_type reflect;
    Bool_type refl_orig_only;
    Bool_type manual_backface_cull;
    Bool_type displace_exag;
    float displace_scale[3];

    Result *cur_result;
    /* Added by Bill Oliver Jan 2014 for needed logic to show mat color combined with
 *     the "show mat" command */
    Bool_type showmat;

    int result_index;
    char result_title[80];

    int max_result_size;
    float *tmp_result[TEMP_RESULT_ARRAY_QTY]; /* Cache for result data. */
    float zero_result;
    Strain_type strain_variety;
    Damage_vals damage_params;
    Ref_surf_type ref_surf;
    Ref_frame_type ref_frame;

    Bool_type use_global_mm;
    Bool_type use_cumulative_mm;
    Bool_type found_global_mm;
    Bool_type result_mod;
    float result_mm[2];            /* For result at nodes. */
    float time_mm[2];
    float state_mm[2];             /* For result at nodes. */
    int state_mm_nodes[2];         /* For result at nodes. */
    char *state_mm_class_long_name[2]; /* For nodal result. */
    int  state_mm_sclass[2];
    float global_mm[2];            /* For result at nodes. */
    int global_mm_sclass[2];
    int global_mm_nodes[2];        /* For result at nodes. */
    char *global_mm_class_long_name[2]; /* For nodal result. */
    Minmax_obj *result_mm_list;    /* Cache for global min/max's (at nodes). */
    Bool_type mm_result_set[2], mm_time_set[2];
    Minmax_obj elem_global_mm;     /* For result on element. */
    Minmax_obj elem_state_mm;      /* For result on element. */
    Minmax_obj tmp_elem_mm;        /* For result on element. */
    Hash_table *prev_mm_globals; 		/* hash table to store previously found global mm's*/

    Plot_obj *current_plots;
    Result *abscissa;
    int first_state; /* On [0, qty - 1], temporary use _only_ for plots. */
    int last_state;  /* On [0, qty - 1], temporary use _only_ for plots. */
    Time_series_obj *times;        /* Temporary use _only_ for plots. */
    int *srec_formats;
    List_head *format_blocks;

    Time_series_obj *time_series_list;
    Time_series_obj *oper_time_series_list;
    Result *series_results;

    Bool_type show_plot_grid;
    Bool_type show_plot_glyphs;
    Bool_type show_plot_coords;
    Bool_type show_legend_lines;
    Bool_type color_plots;
    Bool_type color_glyphs;
    char minmax_loc[2];

    Bool_type th_smooth;
    int th_filter_width;
    Filter_type th_filter_type;

    Bool_type zbias_beams;
    float beam_zbias;

    Bool_type z_buffer_lines;
    float z_poly_offset;
    float z_poly_last;

    Bool_type show_edges, show_edges_vec, hide_edges_by_mat;
    float edge_width;
    float edge_zbias;

    int contour_cnt;
    float *contour_vals;
    Bool_type show_contours;
    Contour_obj *contours;
    float contour_width;
    Bool_type show_isosurfs;
    Triangle_poly *isosurf_poly;

    Plane_obj *cut_planes;
    Bool_type show_roughcut;
    Bool_type show_particle_cut;
    Bool_type show_cut;
    Classed_list *cut_poly_lists;

    Bool_type show_vectors;
    Bool_type show_vector_spheres;
    Result *vector_result[3];
    List_head *vector_srec_map;
    Bool_type vec_col_set;
    Bool_type vec_hd_col_set;
    double vec_min_mag;
    double vec_max_mag;
    float vec_scale;
    float vec_head_scale;
    Bool_type have_grid_points;
    Bool_type vectors_at_nodes;
    Bool_type scale_vec_by_result;

    float dir_vec[3];

    Bool_type do_tensor_transform;
    float (*tensor_transform_matrix)[3];

    Bool_type show_traces;
    Trace_pt_def_obj *trace_pt_defs;
    Trace_pt_obj *trace_pts;
    Trace_pt_obj *new_trace_pts;
    float trace_delta_t;
    int ptrace_limit;
    int trace_disable_qty;
    int *trace_disable;
    float trace_width;
    float point_diam;

    Bool_type show_extern_polys;
    Surf_poly *extern_polys;
    Bool_type show_ref_polys;
    Bool_type result_on_refs;
    Ref_poly *ref_polys;

    Bool_type perform_unit_conversion;
    float conversion_scale;
    float conversion_offset;

    Bool_type auto_img;
    Autoimg_format_type autoimg_format;
    char *rgb_root;
    char *img_root;
    Bool_type loc_ref;
    Bool_type show_num;
    char *num_class;
    MO_class_data **classarray;
    int classqty;

    Bool_type preview_mode;

    //last colors
    Bool_type lastColorActive;
    float** last_ambient;
    float** last_diffuse;
    float** last_specular;
    float** last_emission;
    float* last_shininess;

    //cancel colors
    Bool_type defaultColorActive;
    float** default_ambient;
    float** default_diffuse;
    float** default_specular;
    float** default_emission;
    float* default_shininess;


    Bool_type show_particle_class;

    /*
     * Added April 20, 2004: IRC - Used for setting log scale
     */
    Bool_type logscale;

    /*
     * Added August 13, 2004: IRC - Variables Used for removing damaged
     * elements
     */
    Bool_type damage_hide;
    Bool_type reset_damage_hide;
    Bool_type damage_result;

    /*
     * Added December 22, 2004: IRC - Variables used for displaying
     * free_nodes.
     */
    Bool_type free_nodes_enabled, particle_nodes_hide_background,
              free_nodes_found, particle_nodes_found;

    float     free_nodes_scale_factor;
    int       free_nodes_sphere_res_factor;
    int       *free_nodes_mats;
    int       free_nodes_mass_scaling;
    int       free_nodes_vol_scaling;
    float     free_nodes_cut_width;

    /*
     * Added March 22, 2005: IRC - Variables used for keep global mesh
     *                             info.
     */
    Bool_type  stateDB;  /* If TRUE, then this is a state database
                          * and not a timehistory database.
                          */

    Bool_type   *free_nodes_list;
    float       *free_nodes_vals;

    /*
     * Added January 24, 2006: IRC - Variables used for displaying
     * free_nodes.
     */
    Bool_type  particle_nodes_enabled;

    /*
     * Added January 30, 2006: IRC - Variables used for RubberBand Zoom.
     */
    Bool_type rb_vcent_flag;

    /*
     * Added January 24, 2006: IRC - Variables used for displaying
     * free_nodes.
     */
    Bool_type free_particles;

    /*
     * Added February 07, 2006: IRC - Variables used writing FN momentum
     * data to a text file.
     */
    Bool_type fn_output_momentum;

    /*
     * Added February 07, 2006: IRC - Variables used for drawing wireframe meshes.
     */
    Bool_type draw_wireframe;
    Bool_type draw_wireframe_trans;

    /*
     * Added February 24, 2006: IRC - Variables used for tracking enable and vis
     * of classes.
     */
    Bool_type enable_brick;
    Bool_type hide_brick;

    /*
     * Added July 25, 2007: IRC - Used for viewing materials with grey scale
     *                            colormap.
     */
    Bool_type material_greyscale;

    /*
     * Added August 15, 2007: IRC - Variables used to plot TI variables.
     */
    Bool_type    ti_data_found;
    int          ti_var_count;
    TI_Var       *ti_vars;

    /*
     * Added October 05, 2007: IRC - Error indicator result.
     *
     */
    Bool_type ei_result;
    Bool_type result_active;
    char      ei_result_name[40];

    /* Global EI quantities */

    float ei_error_norm, ei_norm_qty, ei_global_indicator, ei_refinement_indicator;

    /*
     * Added December 18, 2007: IRC - Used to control pausing.
     *
     */
    Bool_type hist_paused;
    char      hist_filename[64];
    int       hist_line_cnt;

    char      curr_result[512];

    Bool_type suppress_updates;

    Bool_type extreme_result, extreme_min, extreme_max;

    float *extreme_node_mm;

    /*
     * Added October 01, 2011: IRC - Used to turn off adjust near-far warnings.
     *
     */
    Bool_type adjust_NF_disable_warning;
    Bool_type adjust_NF_retry;

    Bool th_plotting;

    /*
     * Added December 08, 2011: IRC - Used to track badly defined sub-records
     * from Diablo. This and associated code will be removed when Diablo Mili
     * output is corrected.
     *
     */
    int num_bad_subrecs, *bad_subrecs;

    /*
     * Added February 1, 2012: IRC - Array to store TimeDep informational
     * messages 1- <infoMsgCnt.
     *
     */
    int infoMsgCnt;
    char *infoMsg[MAXINFOMSG];
    /*
     * Added March 12, 2012: IRC - Flag to show or hide SPH ghost
     * particles.
     *
     */
    Bool_type show_sph_ghost;

    /*
     * Added March 29, 2012: IRC - Path for opening new files with a
     * load command.
     *
     */
    char *paths[MAX_PATHS];
    Bool_type paths_set[MAX_PATHS];

    /*
     * Added April 18, 2012: IRC - Added ehnancement to enforce checking
     * for enabled materials for selections.
     *
     */
    Bool_type selectonmat;

    /*
     * Added April 18, 2012: IRC - Added ehnancement to turn off echo of
     * commands in GUI command window.
     */
    Bool_type echocmd;

    /*
     * Added July 26, 2012: IRC - New variables to support Modal Analysis
     * supported by Diablo.
     */
    ;
    Analysis_type  analysis_type;
    char           *time_name;

    FILE *p_histfile;
    char hist_fname[512];

    /*
     * Added July 26, 2013: IRC - Flag to output TH in single column
     * format and to append writes to output file.
     */
    Bool_type th_single_col, th_append_output;

    /*
     * Added February 11, 2013: IRC - Added on/off flag to disable volume
     * averaging when looking at interp. results.
     */
    Bool_type vol_averaging;

    /*
     * Added February , 2013: IRC - Array of integration point label
     * data.
     */
    int es_cnt; /* Number of element sets */
    Hash_table * Element_sets;
    char **Element_set_names;
    Bool_type int_point_labels;

    /*
 *   Added February , 2014:  WBO Switch to turn off auto gray
 *  */
    Bool_type auto_gray;

    Bool_type old_shell_stresses;

    Bool_type render_particle_results_onto_bg_elements;
}
Analysis;


/*****************************************************************
 * TAG( Session )
 *
 * Struct which contains all the info related to a user session -
 *   This struct contains global and  plot specific session types
 *   of data.
 */
typedef struct _Session
{

    short var_update[100];

    /* Global Session Fields */

    char root_name[512];
    Bool_type limit_max_state;
    int max_state;
    Bool_type limit_min_state;
    int reference_state;
    float *cur_ref_state_data;
    float *ref_state_data;

    Bool_type normals_constant;
    float *node_norm[3];
    Bool_type shared_faces;
    float bbox[2][3];
    int bbox_source_qty;
    MO_class_data **bbox_source;
    Bool_type vis_mat_bbox;
    Bool_type keep_max_bbox_extent;

    Bool_type limit_rotations;

    Bool_type show_background_image;
    RGB_raster_obj *background_image;

    Render_mode_type render_mode;
    Bool_type refresh;
    Bool_type normals_smooth;
    Bool_type hex_overlap;
    Mesh_view_mode_type mesh_view_mode;
    Interp_mode_type interp_mode;
    Bool_type show_bbox;
    Bool_type show_coord;
    Bool_type show_time;
    Bool_type show_title, show_title_path;
    Bool_type show_safe;
    Bool_type show_colormap;
    Bool_type show_colorscale;
    Bool_type show_minmax;
    Bool_type show_scale;
    Bool_type show_datetime;
    Bool_type show_tinfo;
    Bool_type use_colormap_pos;
    float hidden_line_width;
    int float_frac_size;
    Bool_type auto_frac_size;
    float colormap_pos[4];
    Mouse_mode_type mouse_mode;
    MO_class_data *pick_class;
    MO_class_data *hilite_class;
    int hilite_label;
    int hilite_num;
    Center_view_sel_type center_view;
    int center_node;
    float view_center[3];
    Specified_obj *selected_objects;

    Refl_plane_obj *refl_planes;
    Bool_type reflect;
    Bool_type refl_orig_only;
    Bool_type manual_backface_cull;
    Bool_type displace_exag;
    float displace_scale[3];

    Result *cur_result;
    int result_index, result_index_max;
    char result_title[80];

    int max_result_size;
    float *tmp_result[TEMP_RESULT_ARRAY_QTY]; /* Cache for result data. */
    float zero_result;
    Strain_type strain_variety;
    Damage_vals damage_params;
    Ref_surf_type ref_surf;
    Ref_frame_type ref_frame;

    Bool_type use_global_mm;
    Bool_type result_mod;
    float result_mm[2];            /* For result at nodes. */
    float state_mm[2];             /* For result at nodes. */
    int state_mm_nodes[2];         /* For result at nodes. */
    char *state_mm_class[2];       /* For nodal result. */
    float global_mm[2];            /* For result at nodes. */
    int global_mm_nodes[2];        /* For result at nodes. */
    char *global_mm_class[2];      /* For nodal result. */
    Minmax_obj *result_mm_list;    /* Cache for global min/max's (at nodes). */
    Bool_type mm_result_set[2];
    Minmax_obj elem_global_mm;       /* For result on element. */
    Minmax_obj elem_state_mm;      /* For result on element. */
    Minmax_obj tmp_elem_mm;        /* For result on element. */

    Plot_obj *current_plots;
    Result *abscissa;
    int first_state; /* On [0, qty - 1], temporary use _only_ for plots. */
    int last_state;  /* On [0, qty - 1], temporary use _only_ for plots. */
    Time_series_obj *times;        /* Temporary use _only_ for plots. */
    int *srec_formats;
    List_head *format_blocks;

    Time_series_obj *time_series_list;
    Time_series_obj *oper_time_series_list;
    Result *series_results;

    Bool_type show_plot_grid;
    Bool_type show_plot_glyphs;
    Bool_type show_plot_coords;
    Bool_type show_legend_lines;
    Bool_type color_plots;
    Bool_type color_glyphs;
    char minmax_loc[2];

    Bool_type th_smooth;
    int th_filter_width;
    Filter_type th_filter_type;

    Bool_type zbias_beams;
    float beam_zbias;

    Bool_type z_buffer_lines;

    Bool_type show_edges, hide_edges_by_mat;
    float edge_width;
    float edge_zbias;

    int contour_cnt;
    float *contour_vals;
    Bool_type show_contours;
    Contour_obj *contours;
    float contour_width;
    Bool_type show_isosurfs;
    Triangle_poly *isosurf_poly;

    Plane_obj *cut_planes;
    Bool_type show_roughcut;
    Bool_type show_particle_cut;
    Bool_type show_cut;
    Classed_list *cut_poly_lists;

    Bool_type show_vectors;
    Bool_type show_vector_spheres;
    Result *vector_result[3];
    List_head *vector_srec_map;
    Bool_type vec_col_set;
    Bool_type vec_hd_col_set;
    double vec_min_mag;
    double vec_max_mag;
    float vec_scale;
    float vec_head_scale;
    Bool_type have_grid_points;
    Bool_type vectors_at_nodes;
    Bool_type scale_vec_by_result;

    float dir_vec[3];

    Bool_type do_tensor_transform;
    float (*tensor_transform_matrix)[3];

    Bool_type show_traces;
    Trace_pt_def_obj *trace_pt_defs;
    Trace_pt_obj *trace_pts;
    Trace_pt_obj *new_trace_pts;
    float trace_delta_t;
    int ptrace_limit;
    int trace_disable_qty;
    int *trace_disable;
    float trace_width;

    Bool_type show_extern_polys;
    Surf_poly *extern_polys;
    Bool_type show_ref_polys;
    Bool_type result_on_refs;
    Ref_poly *ref_polys;

    Bool_type perform_unit_conversion;
    float conversion_scale;
    float conversion_offset;

    Bool_type auto_img;
    Autoimg_format_type autoimg_format;
    char rgb_root[512];
    char img_root[512];
    Bool_type loc_ref;
    Bool_type show_num;

    Bool_type show_particle_class;

    /*
     * Added April 20, 2004: IRC - Used for setting log scale
     */
    Bool_type logscale;

    /*
     * Added August 13, 2004: IRC - Variables Used for removing damaged
     * elements
     */
    Bool_type damage_hide;
    Bool_type reset_damage_hide;
    Bool_type particle_nodes_hide_background;

    float     free_nodes_scale_factor;
    int       free_nodes_sphere_res_factor;
    int       free_nodes_mass_scaling;
    int       free_nodes_vol_scaling;
    float     free_nodes_cut_width;
    /*
     * Added January 24, 2006: IRC - Variables used for displaying
     * free_nodes.
     */
    Bool_type  particle_nodes_enabled;

    /*
     * Added February 07, 2006: IRC - Variables used writing FN momentum
     * data to a text file.
     */
    Bool_type fn_output_momentum;

    /*
     * Added February 07, 2006: IRC - Variables used for drawing wireframe meshes.
     */
    Bool_type draw_wireframe;
    Bool_type draw_wireframe_trans;

    /*
     * Added January 24, 2006: IRC - Variables used for displaying
     * free_nodes.
     */
    Bool_type free_particles;

    /*
     * Added February 24, 2006: IRC - Variables used for tracking enable and vis
     * of classes.
     */
    Bool_type enable_brick;
    Bool_type hide_brick;

    /*
     * Added July 25, 2007: IRC - Used for viewing materials with grey scale
     *                            colormap.
     */
    Bool_type material_greyscale;

    /*
     * Added October 05, 2007: IRC - Error indicator result.
     *
     */
    Bool_type ei_result;

    /*
     * Added October 01, 2011: IRC - Used to turn off adjust near-far warnings.
     *
     */
    Bool_type adjust_NF_disable_warning;
    Bool_type adjust_NF_retry;

    /*
     * Added May 10, 2007: IRC - Variables for window size and placement.
     */
    int win_render_size[2], win_render_pos[2];
    int win_ctl_size[2],    win_ctl_pos[2];
    int win_util_size[2],   win_util_pos[2],   win_util_active;
    int win_mtl_size[2],    win_mtl_pos[2],    win_mtl_active;
    int win_surf_size[2],   win_surf_pos[2],   win_surf_active;

    Bool_type hist_paused;
    char      hist_filename[256];
    int       hist_line_cnt;

    char      curr_result[512];
} Session;


#define MODEL_VAR 1
#define GLOBAL_VAR 2
#define GET 0
#define PUT 1
#define READ  10
#define WRITE 11

typedef struct
{
    char varname[256];
    int  var_type;
    int  var_len;
    Bool_type model_var;

} analy_var_TOC_type;

typedef enum
{
    PUSHPOP_ABOVE,
    PUSHPOP_BELOW
} PushPop_type;


/*****************************************************************
 * TAG( Environ )
 *
 * The environment struct contains global program variables which
 * aren't associated with a particular analysis.
 */
typedef struct
{
    Analysis *curr_analy;
    Bool_type save_history;
    Bool_type history_input_active;
    Bool_type animate_active, animate_reverse;
    FILE *history_file;
    char input_file_name[512];
    char output_file_name[100];
    char partfile_name_c[256];
    char partfile_name_s[256];
    char plotfile_name[MAXPATHLENGTH];
    char plotfile_prefix[256];
    char user_name[30];
    char date[32], time[32];
    int nprocs;
    int nstates;
    Bool_type timing;
    Bool_type result_timing;
    Bool_type single_buffer;
    Bool_type foreground;

    /* True if glx context uses direct render, false if indirect rendering
     * This is basically checking if we are running on VNC, VNC uses direct rendering
     * while Xwin32 uses indirect rendering.
     */
    Bool_type direct_rendering;

    /*
     * Added January 5, 2005: IRC - Variable used for selecting a beta
     * version of the code to run.
     */
    float   run_alpha_version;
    float   run_beta_version;

    /*
     * Added February 27, 2007: IRC - Variables used for runtime logging.
     */
    Bool_type enable_logging, model_history_logging;
    char grizlogdir[64];
    char grizlogfile[64];
    char host[20];
    char arch[30];
    char systype[30];
    float time_used; /* In seconds */
    time_t time_start, time_stop;
    char command_line[256];
    /*
     * Added August 14, 2007: IRC - Variable used for enabling plotting
     * of TI data.
     */
    Bool_type ti_enable;

    /*
     * Added December 17, 2007: IRC - Variable used for identifying a
     * Windows display architecture.
     */
    Bool_type win32;

    /*
     * Added November 05, 2009: IRC - ProcessId to identify windows.
     */
    int griz_pid;

    long border_colors[20];
    int  griz_id;

    long dialog_text_color;
    Bool_type show_dialog;

    Bool_type window_size_set_on_command_line;

    char *bname;
    Bool_type checkresults;
    /*
     * Added September 09, 2011: IRC - Used for Batch quiet mode.
     *
     */
    Bool_type quiet_mode;
} Environ;

extern Environ env;

#define MAXTOKENS 2000
#define TOKENLENGTH 100

/* viewer.c */
extern Bool_type load_analysis( char *fname, Analysis *analy, Bool_type reload );
extern void close_analysis( Analysis *analy );
extern void open_history_file( char *fname );
extern void close_history_file( void );
extern void history_command( char *command_str );
extern void history_log_command( char *fname );
extern void history_log_clear( );
extern Bool_type read_history_file( char *fname, int line_num, int loop_count,
                                    Analysis *analy );
extern void model_history_log_clear( Analysis * analy );
extern void model_history_log_update( char *command, Analysis *analy );
extern void model_history_log_comment(char *comment, Analysis *analy);
extern void model_history_log_run( Analysis * analy );

#ifdef GRIZ_SERVER_BUILD
extern int process_server_mode_stdio( const char *db_path,
                                      int width, int height );
#endif

extern char * griz_version;
extern char * particle_cname;
extern Database_type_griz db_type;

/* faces.c */
extern int fc_nd_nums[6][4];
extern int tet_fc_nd_nums[4][3];
extern int pyramid_fc_nd_nums[5][4];
extern int edge_face_nums[12][2];
extern int edge_node_nums[12][2];
extern int tet_edge_node_nums[6][2];
extern int pyramid_edge_node_nums[8][2];
extern void update_hex_adj( Analysis *analy );
extern void create_hex_adj( MO_class_data *, Analysis * );
extern void init_hex_visibility( MO_class_data *, Analysis * );
extern void set_hex_visibility( MO_class_data *, Analysis * );
extern void get_hex_faces( MO_class_data *, Analysis * );
extern void create_tet_adj( MO_class_data *, Analysis * );
extern void set_tet_visibility( MO_class_data *, Analysis * );
extern void get_tet_faces( MO_class_data *, Analysis * );
extern void create_quad_adj( MO_class_data *p_mocd, Analysis *analy );
extern void create_tri_adj( MO_class_data *p_mocd, Analysis *analy );
extern void create_pyramid_adj( MO_class_data *p_mocd, Analysis *analy );
extern void set_pyramid_visibility( MO_class_data *, Analysis * );
extern void get_pyramid_faces( MO_class_data *, Analysis * );
extern void free_vis_data( Visibility_data **, int );
extern void reset_face_visibility( Analysis *analy );
extern void get_tet_face_verts( int, int, MO_class_data *, Analysis *,
                                float [][3] );
extern void get_hex_face_verts( int, int, MO_class_data *, Analysis *,
                                float [][3] );
extern void get_pyramid_face_verts( int, int, MO_class_data *, Analysis *,
                                    float [][3] );
void get_hex_face_nodes( int elem, int face, MO_class_data *p_hex_class, Analysis *analy, int *faceNodes );
extern void get_hex_verts( int, MO_class_data *, Analysis *, float [][3] );
extern void get_pyramid_verts( int, MO_class_data *, Analysis *, float [][3] );
extern void get_particle_verts( int, MO_class_data *, Analysis *, float [][3] );
extern void get_tet_verts( int, MO_class_data *, Analysis *, float [][3] );
extern void get_tri_verts_3d( int, MO_class_data *, Analysis *, float[][3] );
extern void get_tri_verts_2d( int, MO_class_data *, Analysis *, float[][3] );
extern void get_quad_verts_3d( int, int *, int, Analysis *, float[][3] );
extern void get_quad_verts_2d( int, MO_class_data *, Analysis *, float[][3] );
extern Bool_type get_beam_verts_2d_3d( int, MO_class_data *, Analysis *,
                                       float [][3] );
extern Bool_type get_truss_verts_2d_3d( int, MO_class_data *, Analysis *,
                                        float [][3] );
extern void get_node_vert_2d_3d( int, MO_class_data *, Analysis *, float [3] );
extern void compute_normals( Analysis *analy );
extern void bbox_nodes( Analysis *, MO_class_data **, Bool_type, float[3],
                        float[3] );
extern void count_materials( /* analy */ );
extern Bool_type face_matches_quad( int, int, MO_class_data *, Mesh_data *,
                                    Analysis * );
extern Bool_type face_matches_tri( int, int, MO_class_data *, Mesh_data *,
                                   Analysis * );
extern int select_item( MO_class_data *, int, int, Bool_type, Analysis * );
extern void get_mesh_edges( int mesh_id, Analysis *analy );
extern void create_hex_blocks( MO_class_data *, Analysis * );
extern void create_tet_blocks( MO_class_data *, Analysis * );
extern void create_quad_blocks( MO_class_data *, Analysis * );
extern void create_tri_blocks( MO_class_data *, Analysis * );
extern void create_pyramid_blocks( MO_class_data *, Analysis *);
extern void free_elem_block( Elem_block_obj **, int );
extern void write_ref_file( char tokens[MAXTOKENS][TOKENLENGTH], int token_cnt,
                            Analysis *analy );
extern void get_extents( Analysis *, Bool_type, Bool_type, float *, float * );


/* io_wrappers.c */
extern State2 *mk_state2( Analysis *, State_rec_obj *, int, int, int,
                          State2 * );
extern void create_subrec_node_list( int *, int, Subrec_obj * );
extern int create_primal_result( Mesh_data *, int, int, Subrec_obj *,
                                 Hash_table *, int, char *, Hash_table *, Analysis * );
extern int load_nodpos( Analysis *, State_rec_obj *, Mesh_data *, int, int,
                        Bool_type, void * );

extern int load_hex_nodpos_timehist( Analysis *, int , int,
                                     int *, int **, GVec3D2P **);
extern int load_quad_nodpos_timehist( Analysis *, int , int,
                                      int *, int **, GVec3D2P **);

extern int load_quad_objids_timehist( Analysis *, int* , int** );

extern char *get_subrecord( Analysis *analy, int num );

extern void *get_st_input_buffer( Analysis *, int, Bool_type, void ** );

extern int get_result_qty( Analysis *, int subrec_id );

/* Mili wrappers. */
extern int mili_db_open( char *, int * );
extern int mili_db_get_geom( int, Mesh_data **, int * );
extern int mili_db_get_st_descriptors( Analysis *, int );
extern int mili_db_set_results( Analysis * );
extern int mili_db_get_state( Analysis *, int, State2 *, State2 **, int * );
extern int mili_db_get_subrec_def( int, int, int, Subrecord * );
extern int mili_db_cleanse_subrec( Subrecord * );
extern int mili_db_cleanse_state_var( State_variable * );

extern int mili_db_get_results( int, int, int, int, char **, void * );
extern int mili_db_get_results_allobjids( int, int, Analysis *,int *, int **);
extern int mili_db_get_results_allsrecs( int, int, Analysis *, int, int *, int, char **, void * );

extern int mili_db_query( int, int, void *, char *, void * );
extern int mili_db_get_title( int, char * );
extern int mili_db_get_dimension( int, int * );
extern int mili_db_set_buffer_qty( int, int, char *, int );
extern int mili_db_close( Analysis * );
extern int mili_db_get_param_array( int, char *, void **);
extern MO_class_data *mili_get_class_ptr( Analysis *analy, int superclass,
        char *class_name );

extern int get_hex_volumes( int dbid, Analysis *analy );

/* Taurus plotfile wrappers. */
extern int taurus_db_open( char *, int * );
extern int taurus_db_get_geom( int, Mesh_data **, int * );
extern int taurus_db_get_subrec_def( int, int, int, Subrecord * );
extern int taurus_db_close( Analysis * );
extern int taurus_db_get_title( int, char * );

/* misc.c */
extern Bool_type is_operation_token( char token[TOKENLENGTH],
                                     Plot_operation_type * );
extern Bool_type object_is_bound( int, MO_class_data *, Subrec_obj * );
extern Bool_type bool_compare_array( int, int *, int * );
extern int get_max_state( Analysis *analy );
extern void fr_state2( State2 *p_state, Analysis *analy );
extern MO_class_data *find_named_class( Mesh_data *p_mesh, char *test_name );
extern MO_class_data *find_class_by_long_name( Mesh_data *p_mesh,
        char *test_lname );
extern void destroy_time_series_obj( Time_series_obj **pp_tso );
extern void cleanse_tso_data( Time_series_obj *p_tso );
extern void build_oper_series_label( Time_series_obj *, Bool_type, char * );
extern void build_result_label( Result *, Bool_type, char * );
extern int svar_field_qty( State_variable *p_sv );
extern void create_class_list( int *, MO_class_data ***, Mesh_data *, int,
                               ... );
extern Bool_type intersect_srec_maps( List_head **, Result * [], int,
                                      Analysis * );
extern Bool_type gen_component_results( Analysis *, Primal_result *, int *,
                                        int, Result [] );
extern void create_elem_class_node_list( Analysis *, MO_class_data *, int *,
        int ** );
extern void gen_material_node_list( Mesh_data *, int, Material_data * );
extern Bool_type reorder_float_array( int, int *, int, float *, float * );
extern Bool_type reorder_double_array( int, int *, int, double *, double * );
extern void reset_autoimg_count();
extern void output_image( Analysis * );
extern void outview( FILE *, Analysis * );
extern void prep_object_class_buffers( Analysis *, Result * );

/* time.c */
extern void change_state( Analysis * );
extern void change_time( float, Analysis * );
extern void parse_animate_command( int, char [MAXTOKENS][TOKENLENGTH],
                                   Analysis * );
extern void continue_animate( Analysis * );
extern Bool_type step_animate( Analysis * );
extern void end_animate( Analysis * );

/* minmax.c */
extern void get_global_minmax( Analysis * );
extern Bool_type tellmm( Analysis *, char *, int, int, Redraw_mode_type * );
extern Redraw_mode_type parse_outmm_command( Analysis *,
        char [MAXTOKENS][TOKENLENGTH],
        int );
extern void
get_extreme_minmax( Analysis *, int, int );
extern void
free_extreme_mm( Analysis * );
extern void
update_extreme_mm( Analysis * );

/* VISUALIZATION */

/* iso_surface.c */
extern void gen_cuts( Analysis * );
extern void gen_isosurfs( Analysis * );
extern float get_tri_average_area( /* analy */ );

/* contour.c */
extern void set_contour_vals( int, Analysis * );
extern void add_contour_val( float, Analysis * );
extern void gen_contours( Analysis * );
extern void free_contours( Analysis * );
extern void report_contour_vals( Analysis * );

/* shape.c */
extern void shape_fns_quad( float, float, float [4] );
extern void shape_derivs_2d( float, float, float [4], float [4] );
extern void shape_fns_hex( float, float, float, float [8] );
extern void shape_derivs_3d( float, float, float, float [8], float [8],
                             float [8] );
extern Bool_type pt_in_hex( float [8][3], float [3], float [3] );
extern Bool_type pt_in_tet( float [4][3], float [3], float [4] );
extern Bool_type pt_in_quad( float [4][3], float [2], float [2] );
extern Bool_type pt_in_tri( float [3][3], float [2], float [3] );

/* flow.c */
extern void gen_trace_points( int, float [3], float [3], float [3],
                              Analysis * );
extern void free_trace_list( Trace_pt_obj ** );
extern void free_trace_points( Analysis * );
extern void particle_trace( float, float, float, Analysis *, Bool_type );
extern Bool_type find_next_subtrace( Bool_type, int, int, Trace_pt_obj *,
                                     Analysis *, int *, int * );
extern void save_particle_traces( Analysis *, char * );
extern void read_particle_traces( Analysis *, char * );
extern void delete_vec_points( Analysis *analy );
extern void gen_vec_points( int, int [3], float [3], float [3],
                            Analysis * );
extern void update_vec_points( Analysis * );
extern void gen_carpet_points( Analysis * );
extern void sort_carpet_points( Analysis *, int, float **, int *, int *,
                                Bool_type );
extern void gen_reg_carpet_points( Analysis * );

/* DISPLAY ROUTINES. */

/* draw.c */
extern char *strain_label[];
extern char *ref_surf_label[];
extern char *ref_frame_label[];
extern void init_mesh_window( Analysis * );
extern void reset_mesh_window( Analysis * );
extern void extend_color_prop_arrays( Color_property *, int, GLfloat rgb_array[][3], int );
extern void get_pick_ray( int, int, float [3], float [3] );
extern void get_pick_pt( int, int, float [3] );
extern void set_orthographic( Bool_type val );
extern void set_mesh_view( void );
extern void aspect_correct( Bool_type );
extern void set_camera_angle( float, Analysis * );
extern void inc_mesh_rot( float, float, float, float, float, float, float );
extern void inc_mesh_trans( float, float, float );
extern void set_mesh_scale( float, float, float );
extern void get_mesh_scale( float [3] );
extern void reset_mesh_transform( void );
extern void set_view_to_bbox( float bb_lo[3], float bb_hi[3], int view_dimen );
extern void look_from( float [3] );
extern void look_at( float [3] );
extern void look_up( float [3] );
extern void move_look_from( int, float );
extern void move_look_at( int, float );
extern void adjust_near_far( Analysis * );
extern void set_near( float );
extern void set_far( float );
extern void center_view( Analysis * );
extern void view_transf_mat( Transf_mat * );
extern void inv_view_transf_mat( Transf_mat * );
extern void print_view( void );
extern void define_color_properties( Color_property *, int *, int,
                                     GLfloat [][3], int );
extern void change_current_material( int );
extern void update_current_material( Color_property *,
                                     Material_property_type );
extern Redraw_mode_type set_material( int, char [][TOKENLENGTH], int );
extern void set_color( char *, float * );
extern void draw_mesh_view( Analysis * );
extern void get_foreground_window( float *, float *, float * );
extern void quick_draw_mesh_view( Analysis * );
extern void rgb_to_screen( char *, Bool_type, Analysis * );
extern void screen_to_memory( Bool_type, int, int, unsigned char * );
extern void screen_to_rgb( char [], Bool_type );
extern void screen_to_ps( char [] );
extern void read_text_colormap( char *);
extern void hot_cold_colormap( void );
extern void invert_colormap( void );
extern void set_cutoff_colors( Bool_type, Bool_type, Bool_type );
extern void cutoff_colormap( Bool_type, Bool_type );
extern void gray_colormap( Bool_type );
extern void restore_colormap( );
extern void contour_colormap( int );
extern void delete_lights( void );
extern void set_light( int, char [MAXTOKENS][TOKENLENGTH] );
extern void move_light( int, int, float );
extern void set_vid_title( int, char * );
extern void draw_vid_title( void );
extern void copyright( void );
extern void check_interp_mode( Analysis * );
extern void init_swatch( void );
extern void draw_mtl_swatch( float [3] );
extern void set_particle_radius( float );
extern Bool_type disable_by_object_type( MO_class_data *, int, int, Analysis *, float * );
extern Bool_type hide_by_object_type( MO_class_data *, int, int, Analysis *, float * );

/* time_hist.c */
extern void set_glyphs_per_plot( int, Analysis * );
extern void set_glyph_scale( float );
extern void set_glyph_alignment( Glyph_alignment_type );
extern void tell_time_series( Analysis * );
extern void delete_time_series( int, char [][TOKENLENGTH], Analysis * );
extern void destroy_time_series( Time_series_obj ** );
extern void remove_unused_results( Result ** );
extern void draw_plots( Analysis * );
extern void update_plots( Analysis * );
extern void update_plot_objects( Analysis * );
extern void create_plot_objects( int, char [][TOKENLENGTH], Analysis *,
                                 Plot_obj ** );
extern Bool_type element_set_check(Analysis *);
extern void create_oper_plot_objects( int, char [][TOKENLENGTH], Analysis *,
                                      Plot_obj ** );
extern void output_time_series( int, char [MAXTOKENS][TOKENLENGTH],
                                Analysis * );
extern void parse_gather_command( int, char [MAXTOKENS][TOKENLENGTH],
                                  Analysis * );
extern void init_plot_colors( void );
extern void set_plot_color( int, float * );

/* poly.c */
extern void write_hid_file( char *fname, Analysis *analy );
extern void read_slp_file( char *fname, Analysis *analy );
extern void read_ref_file( char *fname, Analysis *analy );
extern void write_geom_file( char *, Analysis *, Bool_type );

#ifdef HAVE_REF_STUFF
extern void gen_ref_from_coord();
float get_ref_average_area();
#endif

extern void write_obj_file( char *fname, Analysis *analy );
extern void write_vv_connectivity_file();
extern void write_vv_state_file();

/* USER INTERFACE. */

/* interpret.c */

#define MATERIAL_COLOR_CNT 20
extern GLfloat material_colors[MATERIAL_COLOR_CNT][3];

extern void read_token( FILE *infile, char *token, int max_length );
extern void parse_command( char *buf, Analysis *analy );
extern void parse_sigle_command( char *buf, Analysis *analy );
/* Shared mutation of the hilite singleton. Extracted from the "hilite"
 * branch of parse_command() so the server-side pick_at handler can reuse
 * the same state change without re-tokenizing a command string. The
 * id_index is 0-based (matches hilite_num); label is the user-facing id
 * (1-based, or a labels_found alias).
 * planning/ui-design/06-picking-and-queries.md §9 invariant I1. */
extern void griz_set_hilite( Analysis *analy, MO_class_data *p_mo_class,
                             int id_index, int label );
extern void griz_clear_hilite( Analysis *analy );
extern Redraw_mode_type redraw_for_render_mode( Bool_type binding,
        Render_mode_type exclusive,
        Analysis *analy );
extern void manage_render_mode( Render_mode_type new_rmode, Analysis *analy,
                                Bool_type suppress );
extern void update_vis( Analysis * );
extern void create_reflect_mats( Refl_plane_obj *plane );
extern void write_preamble( FILE *ofile );
extern Bool_type is_numeric_token( char *num_string );

/* gui.c */
extern void gui_start( int argc, char **argv, Analysis * );
extern void gui_swap_buffers( void );
extern void init_animate_workproc( void );
extern void end_animate_workproc( Bool_type );
extern void set_window_size( int , int );
extern int get_monitor_width( void );
extern int get_step_stride( void );
extern void put_step_stride( int );
extern void wrt_text( char *, ... );
extern void popup_dialog( int, ... );
extern void clear_popup_dialogs( void );
extern void popup_fatal( char * );
extern void quit( int return_code );
extern void switch_opengl_win( OpenGL_win opengl_win );
extern void destroy_mtl_mgr( void );
extern void update_util_panel( Util_panel_button_type, MO_class_data * );
extern void reset_window_titles( void );
extern void set_alt_cursor( Cursor_type cursor_type );
extern void unset_alt_cursor( void );
extern void set_pick_class( MO_class_data *, Util_panel_button_type );
extern void regenerate_result_menus( void );
extern void regenerate_pick_menus( void );
extern void wrt_standard_db_text( Analysis *, Bool_type );
extern void set_motion_threshold( float );
extern void init_btn_pick_classes( void );
extern void update_gui( Analysis *, Render_mode_type, Render_mode_type );
extern void manage_plot_cursor_display( Bool_type, Render_mode_type,
                                        Render_mode_type );
extern void update_cursor_vals( void );
extern void set_plot_win_params( float, float, float, float, float *, float * );
extern void suppress_display_updates( Bool_type );
extern void write_start_text( void );

extern char * make_path_str( Analysis * analy );
extern char * make_griz_name( Analysis * analy, char * vstr );

/* DERIVED VARIABLES. */

/* result_data.c */
extern void reset_zero_vol_warning();
extern int lookup_result_name( char * );
extern void elem_get_minmax( float *, int, Analysis * );
extern void unit_get_minmax( float *, Analysis * );
extern void mat_get_minmax( float *, Analysis * );
extern void mesh_get_minmax( float *, Analysis * );
extern float hex_vol( float [8], float [8], float [8] );
extern float hex_vol_exact( float [8], float [8], float [8] );
extern void hex_to_nodal( float *, float *, MO_class_data *, int, int *,
                          Analysis * );
extern void init_nodal_min_max( Analysis * );
extern float tet_vol( float [4][3] );
extern void tet_to_nodal( float *, float *, MO_class_data *, int, int *,
                          Analysis * );
extern void quad_to_nodal( float *, float *, MO_class_data *, int, int *,
                           Analysis *, Bool_type );
extern void tri_to_nodal( float *, float *, MO_class_data *, int, int *,
                          Analysis *, Bool_type );
extern void surf_to_nodal( float *, float *, MO_class_data *, int, int *,
                           Analysis *, Bool_type );
extern void beam_to_nodal( float *, float *, MO_class_data *, int, int *,
                           Analysis * );
extern void truss_to_nodal( float *, float *, MO_class_data *, int, int *,
                            Analysis * );
extern void particle_to_nodal( float *, float *, MO_class_data *, int, int *,
                               Analysis * );
extern void init_mm_obj( Minmax_obj * );
extern void load_result( Analysis *, Bool_type, Bool_type, Bool_type );
extern Bool_type load_subrecord_result( Analysis *, int, Bool_type, Bool_type );
extern void dump_result( Analysis *, char * );

/* results.c */
extern Result_candidate possible_results[];
extern void update_result( Analysis *, Result * );
extern Result * create_result_list(char *, Analysis *);
extern void delete_result_list(Result **, Analysis *);
extern Result * insert_result(Result *, Result, Analysis *);
extern int find_result( Analysis *, Result_table_type, Bool_type, Result *,
                        char * );
extern Bool_type search_result_tables( Analysis *, Result_table_type, char *,
                                       char *, int *, int [], char *, int *,
                                       Bool_type *, Derived_result **,
                                       Primal_result **, List_head ** );
extern void init_result( Result * );
extern void cleanse_result( Result * );
extern void delete_result( Result ** );
extern Result *duplicate_result( Analysis *, Result *, Bool_type );
extern Bool_type result_has_class( Result *, MO_class_data *, Analysis * );
extern Bool_type result_has_superclass( Result *, int, Analysis * );
extern void load_stress_local_coord(Analysis *, float *, Bool_type);
extern void load_primal_result( Analysis *, float *, Bool_type );
extern void load_primal_result_double( Analysis *, float *, Bool_type );
extern void load_primal_result_int( Analysis *, float *, Bool_type );
extern void load_primal_result_long( Analysis *, float *, Bool_type );
extern Bool_type parse_result_spec( char *, char *, int *, int [], char * );
extern Bool_type mod_required_mesh_mode( Analysis *, Result_modifier_type, int,
        int );
extern Bool_type mod_required_plot_mode( Analysis *, Result_modifier_type, int,
        int );
extern int get_element_set_id( char * );
extern int get_element_set_index( Analysis *, int );
extern int get_intpoint_index ( int, int, int * );
extern void set_default_intpoints ( int, int, int *, int * );
extern void get_intpoints ( Analysis *, int, int[3] );

/* show.c */
extern int parse_show_command( char *, Analysis * );
extern Bool_type refresh_shown_result( Analysis *analy );
extern void cache_global_minmax( Analysis * );
extern Redraw_mode_type reset_global_minmax( Analysis * );
extern Bool_type match_result( Analysis *, Result *, Result_spec * );

/* strain.c */
extern void free_static_strain_data( void );
extern void compute_hex_strain( Analysis *, float *, Bool_type );
extern void compute_hex_strain_2p( Analysis *, float *, Bool_type );
extern void compute_hex_eff_strain( Analysis *, float *, Bool_type );
extern void compute_hex_relative_volume( Analysis *, float *, Bool_type );
extern void compute_shell_strain( Analysis *, float *, Bool_type );
extern void compute_shell_eff_strain( Analysis *, float *, Bool_type );
extern void compute_beam_axial_strain( Analysis *, float *, Bool_type );
extern void set_diameter( float );
extern void set_youngs_mod( float );

extern void compute_strain_invariant_one( Analysis*, float*, Bool_type );
extern void compute_strain_inv_one( Analysis*, float*, Bool_type );
extern void compute_es_strain_inv_one( Analysis*, float*, Bool_type );
extern void compute_strain_invariant_two( Analysis*, float*, Bool_type );
extern void compute_strain_inv_two( Analysis*, float*, Bool_type );
extern void compute_es_strain_inv_two( Analysis*, float*, Bool_type );
extern void compute_principal_strain( Analysis*, float*, Bool_type );
extern void compute_prin_strain( Analysis*, float*, Bool_type );
extern void compute_es_prin_strain( Analysis*, float*, Bool_type );

extern Bool_type is_primal_quad_strain_result( char *result_name );
extern void      rotate_quad_result( Analysis *analy, char *primal, int result_cnt,
                                     float *result );


/* stress.c */
extern char* get_es_name(Primal_result* primal_result, int subrec);
extern char* build_es_stress_strain_query_string(char* es_name, int ipt, Bool_type new_vector_array_format, int component, Bool_type strain);
extern void compute_hex_stress_wrapper( Analysis *, float *, Bool_type );
extern void compute_hex_stress( Analysis *, float *, Bool_type );
extern void compute_hex_es_stress( Analysis *, float *, Bool_type );

extern void compute_pressure( Analysis *, float *, Bool_type );
extern void compute_press( Analysis *, float *, Bool_type );
extern void compute_es_press( Analysis *, float *, Bool_type );
extern void compute_shell_press( Analysis *, float *, Bool_type );

extern void compute_effective_stress( Analysis *, float *, Bool_type );
extern void compute_effstress( Analysis *, float *, Bool_type );
extern void compute_es_effstress( Analysis *, float *, Bool_type );
extern void compute_shell_effstress( Analysis *, float *, Bool_type );

extern void compute_principal_stress( Analysis *, float *, Bool_type );
extern void compute_prin_stress( Analysis *, float *, Bool_type );
extern void compute_es_prin_stress( Analysis *, float *, Bool_type );
extern void compute_shell_principal_stress( Analysis *, float *, Bool_type );

extern void compute_shell_surface_stress( Analysis *, float *, Bool_type );
extern void compute_shell_stress( Analysis *, float *, Bool_type );

/* node.c */
extern void compute_node_displacement( Analysis *, float *, Bool_type );
extern void compute_node_radial_displacement( Analysis *, float *, Bool_type );
extern void compute_node_displacement_mag( Analysis *, float *, Bool_type );
extern void compute_node_modaldisplacement_mag( Analysis *, float *, Bool_type );
extern void compute_node_velocity( Analysis *, float *, Bool_type );
extern void compute_node_acceleration( Analysis *, float *, Bool_type );
extern void compute_node_pr_intense( Analysis *, float *, Bool_type );
extern void compute_node_helicity( Analysis *, float *, Bool_type );
extern void compute_node_enstrophy( Analysis *, float *, Bool_type );
extern Bool_type check_compute_vector_component( Analysis * );
extern void compute_vector_component( Analysis *, float *, Bool_type );
extern void set_pr_ref( float pr_ref );
extern void get_class_nodes ( int superclass, Mesh_data *p_mesh,
                              int *num_nodes, int *node_list );

/* global.c */
extern void compute_global_acceleration( Analysis *, float *, Bool_type );
/* frame.c */

extern void global_to_local_mtx( Analysis *, MO_class_data *, int, Bool_type, GVec3D2P *, float [3][3] );
extern void global_to_local_tri_mtx( Analysis *, MO_class_data *, int, Bool_type, GVec3D2P *, float [3][3] );
extern Bool_type transform_stress_strain( char **, int, Analysis *, float [3][3], float * );
extern void transform_tensors( int, double (*)[6], float [][3] );
extern void transform_tensors_1p( int, float (*)[6], float [][3] );

/* explode.c */
extern int associate_matl_exp( int token_cnt, char tokens[MAXTOKENS][TOKENLENGTH], Analysis *analy, Exploded_view_type exp );
extern void explode_materials( int token_cnt, char tokens[MAXTOKENS][TOKENLENGTH], Analysis *analy, Bool_type scaled );
extern void free_matl_exp( void );
extern void remove_exp_assoc( int token_cnt, char tokens[MAXTOKENS][TOKENLENGTH] );
extern void report_exp_assoc( void );

/* init_io.c */
extern void parse_griz_init_file( void ); // maybe into interpret.c instead?
extern Bool_type is_known_db( char *fname, Database_type_griz *p_db_type );
extern Bool_type init_db_io( Database_type_griz db_type, Analysis *analy );
extern void reset_db_io( Database_type_griz db_type );

/* tell.c */
extern void parse_tell_command( Analysis *analy, char tokens[][TOKENLENGTH], int token_cnt, Bool_type ignore_tell_token, Redraw_mode_type *p_redraw );

/* damage.c */
extern void compute_hex_damage( Analysis *analy, float *resultArr, Bool_type interpolate );

/* free node functions  - an = all nodes counted, not just free nodes */

extern int *get_free_nodes( Analysis *analy );
extern void compute_fnmass( Analysis *analy, float *resultArr, Bool_type interpolate );
extern void compute_anmass( Analysis *analy, float *resultArr, Bool_type interpolate );
extern void compute_fnmoment( Analysis *analy, float *resultArr, Bool_type interpolate );
extern void compute_anmoment( Analysis *analy, float *resultArr, Bool_type interpolate );
extern void compute_fnvol(  Analysis *analy, float *resultArr, Bool_type interpolate );
extern void compute_anvol(  Analysis *analy, float *resultArr, Bool_type interpolate );

extern void write_griz_session_file( Analysis *analy, Session *session, char *sessionFileName, int session_id, Bool_type global );
extern int read_griz_session_file( Session *session, char *sessionFileName, int session_id, Bool_type global );

char *get_VersionInfo( Analysis * );
extern char *bi_date(void);
extern char *bi_system(void);
extern char *bi_developer(void);

extern void check_for_free_nodes( Analysis *analy );

extern int is_node_visible( Analysis *analy, int nd );

extern void  VersionInfo(Bool_type quiet_mode);
extern Analysis *get_analy_ptr( );

Bool_type is_particle_class( Analysis *analy, int sclass, char *class );
Bool_type is_dbc_class( Analysis *analy, int sclass, char *class );
/*Bool_type is_elem_class( Analysis *analy, char *class );*/
Bool_type is_elem_class( int superclass );

char *
replace_string( char *input_str, char *sub_str, char *replace_str );

void
update_file_path( Analysis *analy, char *filename, char *filename_with_path );
extern void draw_grid(Analysis * );
extern void draw_grid_2d(Analysis * );
int
get_class_label( MO_class_data *, int  );

#endif
