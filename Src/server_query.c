/*
 * server_query.c - q_* query-command builders for griz-server.
 *
 * Extracted from viewer.c as part of the UI server refactor (planning
 * phase 1, step 1-4). Each builder reads from the Analysis globals
 * that interpret.c mutates and returns a cJSON fragment matching the
 * canonical state schema documented in
 * planning/shared/query-commands.md.
 *
 * server_try_query() is the router: given a raw command line, it
 * matches the q_* keyword and emits a data response on hit, else
 * returns 0 so the caller can fall through to parse_command().
 */

#ifdef GRIZ_SERVER_BUILD

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "viewer.h"
#include "mesh.h"
#include "draw.h"
#include "cJSON.h"
#include "server_core.h"
#include "server_query.h"

extern int  get_window_width(  void );
extern int  get_window_height( void );

/* Trim leading whitespace and return a pointer to the first non-space
 * character. Used to normalize query command strings. */
static const char *
server_skip_ws( const char *s )
{
    while ( *s == ' ' || *s == '\t' )
        s++;
    return s;
}

/* --------------------------------------------------------------------
 * time
 * ------------------------------------------------------------------ */
static cJSON *
build_q_time( Analysis *analy )
{
    cJSON *data = cJSON_CreateObject();
    int    max_state = analy->state_count > 0 ? analy->state_count - 1 : 0;
    double time_value     = 0.0;
    double max_time_value = 0.0;

    if ( analy->state_times != NULL && analy->state_count > 0 )
    {
        int cur = analy->cur_state;
        if ( cur < 0 )                      cur = 0;
        if ( cur >= analy->state_count )    cur = analy->state_count - 1;
        time_value     = analy->state_times[cur];
        max_time_value = analy->state_times[analy->state_count - 1];
    }

    /* Legacy (shipped) keys. */
    cJSON_AddNumberToObject( data, "time_state",     analy->cur_state );
    cJSON_AddNumberToObject( data, "max_time_state", max_state );
    cJSON_AddNumberToObject( data, "state_count",    analy->state_count );
    cJSON_AddNumberToObject( data, "time_value",     time_value );
    cJSON_AddNumberToObject( data, "max_time_value", max_time_value );

    /* Schema keys (planning/shared/query-commands.md). The schema uses
     * 1-based state indices; cur_state is 0-based internally. */
    cJSON_AddNumberToObject( data, "state",     analy->cur_state + 1 );
    cJSON_AddNumberToObject( data, "state_min", 1 );
    cJSON_AddNumberToObject( data, "state_max", analy->state_count );
    cJSON_AddNumberToObject( data, "time",      time_value );
    cJSON_AddBoolToObject(   data, "animating", env.animate_active );

    return data;
}

void *
build_q_time_data( Analysis *analy )
{
    return build_q_time( analy );
}

/* --------------------------------------------------------------------
 * view
 * ------------------------------------------------------------------ */

/* Decode rotation matrix v_win->rot_mat into Euler angles (X-Y-Z
 * convention, degrees) for the schema. Griz itself stores only the
 * accumulated matrix — commands like `rx` post-multiply into rot_mat —
 * so this is a derived quantity. */
static void
rot_mat_to_euler_deg( const Transf_mat *m,
                      double *x_out, double *y_out, double *z_out )
{
    double rx, ry, rz;
    double sy, cy;
    const float (*M)[4] = (const float (*)[4]) m->mat;
    const double RAD2DEG = 57.29577951308232;

    sy = -M[0][2];
    if ( sy >  1.0 ) sy =  1.0;
    if ( sy < -1.0 ) sy = -1.0;
    ry = asin( sy );
    cy = cos( ry );

    if ( cy > 1e-6 )
    {
        rx = atan2( M[1][2], M[2][2] );
        rz = atan2( M[0][1], M[0][0] );
    }
    else
    {
        /* Gimbal-lock: pick a consistent branch. */
        rx = atan2( -M[2][1], M[1][1] );
        rz = 0.0;
    }

    *x_out = rx * RAD2DEG;
    *y_out = ry * RAD2DEG;
    *z_out = rz * RAD2DEG;
}

static cJSON *
build_q_view( Analysis *analy )
{
    cJSON *data     = cJSON_CreateObject();
    cJSON *viewport = cJSON_CreateObject();
    (void) analy;

    cJSON_AddNumberToObject( viewport, "width",  get_window_width()  );
    cJSON_AddNumberToObject( viewport, "height", get_window_height() );
    cJSON_AddItemToObject(   data,     "viewport", viewport );

    if ( v_win != NULL )
    {
        cJSON *rotate    = cJSON_CreateObject();
        cJSON *translate = cJSON_CreateObject();
        cJSON *scale     = cJSON_CreateObject();
        double rx, ry, rz;

        rot_mat_to_euler_deg( &v_win->rot_mat, &rx, &ry, &rz );
        cJSON_AddNumberToObject( rotate, "x", rx );
        cJSON_AddNumberToObject( rotate, "y", ry );
        cJSON_AddNumberToObject( rotate, "z", rz );
        cJSON_AddItemToObject( data, "rotate", rotate );

        cJSON_AddNumberToObject( translate, "x", v_win->trans[0] );
        cJSON_AddNumberToObject( translate, "y", v_win->trans[1] );
        cJSON_AddNumberToObject( translate, "z", v_win->trans[2] );
        cJSON_AddItemToObject( data, "translate", translate );

        cJSON_AddNumberToObject( scale, "x", v_win->scale[0] );
        cJSON_AddNumberToObject( scale, "y", v_win->scale[1] );
        cJSON_AddNumberToObject( scale, "z", v_win->scale[2] );
        cJSON_AddItemToObject( data, "scale", scale );

        /* Griz does not track a dedicated zoom scalar; the scale vector
         * is the authoritative zoom state. Report the average as a
         * convenience scalar for clients that want one. */
        cJSON_AddNumberToObject( data, "zoom",
            ( v_win->scale[0] + v_win->scale[1] + v_win->scale[2] ) / 3.0 );
    }

    return data;
}

void *
build_q_view_data( Analysis *analy )
{
    return build_q_view( analy );
}

/* --------------------------------------------------------------------
 * materials
 * ------------------------------------------------------------------ */
static cJSON *
build_q_materials( Analysis *analy )
{
    cJSON *data       = cJSON_CreateObject();
    cJSON *mat_array  = cJSON_CreateArray();
    int    mat_qty    = MESH_P( analy )->material_qty;
    unsigned char *hide    = MESH_P( analy )->hide_material;
    unsigned char *disable = MESH_P( analy )->disable_material;
    Color_property *props  = ( v_win != NULL ) ? &v_win->mesh_materials
                                               : NULL;
    int props_size         = ( props != NULL ) ? props->property_array_size
                                               : 0;
    int i;

    cJSON_AddNumberToObject( data, "total", mat_qty );

    for ( i = 0; i < mat_qty; i++ )
    {
        cJSON *mat = cJSON_CreateObject();
        /* User-facing material IDs are 1-based. */
        cJSON_AddNumberToObject( mat, "id", i + 1 );
        cJSON_AddBoolToObject(   mat, "visible",
                                 hide    == NULL || hide[i]    == 0 );
        cJSON_AddBoolToObject(   mat, "enabled",
                                 disable == NULL || disable[i] == 0 );

        /* Color from the active material color property array.
         * mesh_materials.diffuse is a GLVec4* indexed by material. */
        if ( props != NULL && props->diffuse != NULL && i < props_size )
        {
            cJSON *color = cJSON_CreateArray();
            cJSON_AddItemToArray( color,
                cJSON_CreateNumber( props->diffuse[i][0] ) );
            cJSON_AddItemToArray( color,
                cJSON_CreateNumber( props->diffuse[i][1] ) );
            cJSON_AddItemToArray( color,
                cJSON_CreateNumber( props->diffuse[i][2] ) );
            cJSON_AddItemToObject( mat, "color", color );
        }
        else
        {
            cJSON_AddNullToObject( mat, "color" );
        }

        /* Label: Griz material labels are stored in hash tables with
         * non-trivial lookup semantics and are not always populated. Use
         * the stable "mat <N>" fallback; downstream clients (MCP, Qt)
         * can translate further if they know the user's naming
         * convention. Full label lookup lands alongside the materials
         * dialog in a later phase. */
        {
            char label[32];
            snprintf( label, sizeof( label ), "mat %d", i + 1 );
            cJSON_AddStringToObject( mat, "label", label );
        }

        cJSON_AddItemToArray( mat_array, mat );
    }

    cJSON_AddItemToObject( data, "materials", mat_array );
    return data;
}

void *
build_q_materials_data( Analysis *analy )
{
    return build_q_materials( analy );
}

/* --------------------------------------------------------------------
 * results
 * ------------------------------------------------------------------ */
static cJSON *
build_q_results( Analysis *analy )
{
    cJSON *data = cJSON_CreateObject();
    cJSON *results_array = cJSON_CreateArray();

    /* Iterate the primal and derived result hash tables.
     * The helper in results.c appends {name, title, origin} objects. */
    server_build_results_from_htable(
        results_array, analy->primal_results, "primal" );
    server_build_results_from_htable(
        results_array, analy->derived_results, "derived" );

    cJSON_AddItemToObject( data, "results", results_array );

    /* Current result, if any. */
    if ( analy->cur_result != NULL && analy->cur_result->name[0] != '\0' )
    {
        cJSON *current = cJSON_CreateObject();
        cJSON_AddStringToObject( current, "name",  analy->cur_result->name );
        cJSON_AddStringToObject( current, "title", analy->cur_result->title );
        cJSON_AddItemToObject( data, "current", current );
    }
    else
    {
        cJSON_AddNullToObject( data, "current" );
    }

    return data;
}

void *
build_q_results_data( Analysis *analy )
{
    return build_q_results( analy );
}

/* --------------------------------------------------------------------
 * render (new)
 * ------------------------------------------------------------------ */
static const char *
render_mode_name( Mesh_view_mode_type m )
{
    switch ( m )
    {
        case RENDER_FILLED:         return "solid";
        case RENDER_HIDDEN:         return "hidden";
        case RENDER_WIREFRAME:      return "wireframe";
        case RENDER_WIREFRAMETRANS: return "wireframe_trans";
        case RENDER_GS:             return "greyscale";
        case RENDER_POINT_CLOUD:    return "point_cloud";
        case RENDER_NONE:           return "none";
    }
    return "unknown";
}

static cJSON *
build_q_render( Analysis *analy )
{
    cJSON *data    = cJSON_CreateObject();
    cJSON *toggles = cJSON_CreateObject();

    cJSON_AddStringToObject( data, "mode",
        render_mode_name( analy->mesh_view_mode ) );

    cJSON_AddBoolToObject( toggles, "coord",  analy->show_coord );
    cJSON_AddBoolToObject( toggles, "time",   analy->show_time );
    cJSON_AddBoolToObject( toggles, "cmap",   analy->show_colormap );
    cJSON_AddBoolToObject( toggles, "minmax", analy->show_minmax );
    cJSON_AddBoolToObject( toggles, "title",  analy->show_title );
    cJSON_AddBoolToObject( toggles, "bbox",   analy->show_bbox );
    cJSON_AddBoolToObject( toggles, "edges",  analy->show_edges );
    cJSON_AddItemToObject( data,    "toggles", toggles );

    /* Griz does not currently expose a colormap name; report null so
     * clients can show "default" and migrate when the colormap-name
     * plumbing lands. */
    cJSON_AddNullToObject( data, "colormap" );

    return data;
}

void *
build_q_render_data( Analysis *analy )
{
    return build_q_render( analy );
}

/* --------------------------------------------------------------------
 * database (new)
 * ------------------------------------------------------------------ */
static cJSON *
build_q_database( Analysis *analy )
{
    cJSON *data = cJSON_CreateObject();
    int    n_nodes    = 0;
    int    n_elements = 0;
    int    n_states   = analy->state_count;
    int    mesh_qty   = analy->mesh_qty;
    int    m;

    if ( analy->mesh_table != NULL )
    {
        for ( m = 0; m < mesh_qty; m++ )
        {
            Mesh_data *mesh = analy->mesh_table + m;
            int        s;
            if ( mesh->node_geom != NULL )
                n_nodes += mesh->node_geom->qty;
            for ( s = 0; s < QTY_SCLASS; s++ )
            {
                List_head *lh = &mesh->classes_by_sclass[s];
                int        k;
                if ( s == G_NODE )
                    continue;
                for ( k = 0; k < lh->qty; k++ )
                {
                    MO_class_data *cd =
                        ((MO_class_data **) lh->list)[k];
                    if ( cd != NULL )
                        n_elements += cd->qty;
                }
            }
        }
    }

    cJSON_AddStringToObject( data, "path",
        env.plotfile_name[0] != '\0' ? env.plotfile_name : "" );
    cJSON_AddBoolToObject(   data, "open",
                             env.plotfile_name[0] != '\0' );
    cJSON_AddNullToObject(   data, "format_version" );
    cJSON_AddNumberToObject( data, "n_nodes",    n_nodes );
    cJSON_AddNumberToObject( data, "n_elements", n_elements );
    cJSON_AddNumberToObject( data, "n_states",   n_states );
    cJSON_AddNumberToObject( data, "mesh_qty",   mesh_qty );

    return data;
}

void *
build_q_database_data( Analysis *analy )
{
    return build_q_database( analy );
}

/* --------------------------------------------------------------------
 * selection (new)
 * ------------------------------------------------------------------ */
static cJSON *
build_q_selection( Analysis *analy )
{
    cJSON *data   = cJSON_CreateObject();
    cJSON *picked = cJSON_CreateArray();
    Specified_obj *obj;
    int count = 0;
    const int max_inline = 1024;

    for ( obj = analy->selected_objects; obj != NULL; obj = obj->next )
    {
        if ( count < max_inline )
        {
            cJSON *entry = cJSON_CreateObject();
            const char *kind =
                ( obj->mo_class != NULL
                  && obj->mo_class->short_name != NULL
                  && obj->mo_class->short_name[0] != '\0' )
                ? obj->mo_class->short_name
                : "element";
            cJSON_AddStringToObject( entry, "kind", kind );
            cJSON_AddNumberToObject( entry, "id",   obj->ident + 1 );
            if ( obj->label != 0 && obj->label != obj->ident + 1 )
                cJSON_AddNumberToObject( entry, "label", obj->label );
            cJSON_AddItemToArray( picked, entry );
        }
        count++;
    }

    cJSON_AddItemToObject(   data, "picked",          picked );
    cJSON_AddNumberToObject( data, "selection_count", count );
    cJSON_AddBoolToObject(   data, "truncated",       count > max_inline );

    /* highlighted: singleton driven by hilite_class/hilite_num. */
    if ( analy->hilite_class != NULL && analy->hilite_num >= 0 )
    {
        cJSON *hl = cJSON_CreateObject();
        cJSON_AddStringToObject( hl, "kind",
            analy->hilite_class->short_name != NULL
                ? analy->hilite_class->short_name : "element" );
        cJSON_AddNumberToObject( hl, "id", analy->hilite_num + 1 );
        if ( analy->hilite_label != 0
             && analy->hilite_label != analy->hilite_num + 1 )
            cJSON_AddNumberToObject( hl, "label",
                analy->hilite_label );
        cJSON_AddItemToObject( data, "highlighted", hl );
    }
    else
    {
        cJSON_AddNullToObject( data, "highlighted" );
    }

    return data;
}

void *
build_q_selection_data( Analysis *analy )
{
    return build_q_selection( analy );
}

/* --------------------------------------------------------------------
 * state (composite)
 * ------------------------------------------------------------------ */
static cJSON *
build_q_state( Analysis *analy )
{
    cJSON *data = cJSON_CreateObject();

    cJSON_AddNumberToObject( data, "schema_version", 1 );

    cJSON_AddItemToObject( data, "database",  build_q_database(  analy ) );
    cJSON_AddItemToObject( data, "time",      build_q_time(      analy ) );
    cJSON_AddItemToObject( data, "view",      build_q_view(      analy ) );
    cJSON_AddItemToObject( data, "render",    build_q_render(    analy ) );
    cJSON_AddItemToObject( data, "materials", build_q_materials( analy ) );
    cJSON_AddItemToObject( data, "results",   build_q_results(   analy ) );
    cJSON_AddItemToObject( data, "selection", build_q_selection( analy ) );

    /* Legacy compatibility keys: shipped stdio clients read these at
     * the top level of the q_state response. Keep until MCP clients
     * migrate to the nested schema. */
    {
        cJSON *viewport = cJSON_CreateObject();
        cJSON_AddNumberToObject( viewport, "width",  get_window_width()  );
        cJSON_AddNumberToObject( viewport, "height", get_window_height() );
        cJSON_AddItemToObject(   data,     "viewport", viewport );

        if ( analy->cur_result != NULL
             && analy->cur_result->name[0] != '\0' )
            cJSON_AddStringToObject( data, "current_field",
                                     analy->cur_result->name );
        else
            cJSON_AddNullToObject(   data, "current_field" );

        if ( analy->result_title[0] != '\0' )
            cJSON_AddStringToObject( data, "result_title",
                                     analy->result_title );
    }

    return data;
}

void *
build_q_state_data( Analysis *analy )
{
    return build_q_state( analy );
}

/* --------------------------------------------------------------------
 * dispatcher
 * ------------------------------------------------------------------ */
int
server_try_query( const char *id, const char *cmd, Analysis *analy )
{
    const char *c;
    cJSON      *data = NULL;

    c = server_skip_ws( cmd );

    if ( strcmp( c, "q_state" ) == 0 )
        data = build_q_state( analy );
    else if ( strcmp( c, "q_view" ) == 0 )
        data = build_q_view( analy );
    else if ( strcmp( c, "q_time" ) == 0 )
        data = build_q_time( analy );
    else if ( strcmp( c, "q_materials" ) == 0 )
        data = build_q_materials( analy );
    else if ( strcmp( c, "q_results" ) == 0 )
        data = build_q_results( analy );
    else if ( strcmp( c, "q_selection" ) == 0 )
        data = build_q_selection( analy );
    else if ( strcmp( c, "q_render" ) == 0 )
        data = build_q_render( analy );
    else if ( strcmp( c, "q_database" ) == 0 )
        data = build_q_database( analy );
    else
        return 0;

    server_emit_data_response( id, data );
    return 1;
}

#endif /* GRIZ_SERVER_BUILD */
