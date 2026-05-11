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
    double min_time_value = 0.0;
    double max_time_value = 0.0;

    /* analy->state_times is declared but never populated anywhere in the
     * tree — query the database directly. QRY_STATE_TIME takes a 1-based
     * state number and writes a float (matches flow.c / gui.c usage). */
    if ( analy->state_count > 0 && analy->db_query != NULL )
    {
        int cur = analy->cur_state;
        int q;
        float t = 0.0f;

        if ( cur < 0 )                      cur = 0;
        if ( cur >= analy->state_count )    cur = analy->state_count - 1;

        q = cur + 1;
        analy->db_query( analy->db_ident, QRY_STATE_TIME,
                         (void *) &q, NULL, (void *) &t );
        time_value = t;

        q = 1;
        analy->db_query( analy->db_ident, QRY_STATE_TIME,
                         (void *) &q, NULL, (void *) &t );
        min_time_value = t;

        q = analy->state_count;
        analy->db_query( analy->db_ident, QRY_STATE_TIME,
                         (void *) &q, NULL, (void *) &t );
        max_time_value = t;
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
    cJSON_AddNumberToObject( data, "time_min",  min_time_value );
    cJSON_AddNumberToObject( data, "time_max",  max_time_value );
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
    /* Vocabulary expansion (planning/render-toggles.md R7): expose every
     * plot-decoration toggle the `on`/`off` parser in interpret.c
     * accepts so RenderAPI can read back what it sets. */
    cJSON_AddBoolToObject( toggles, "path",   analy->show_title_path );
    cJSON_AddBoolToObject( toggles, "cscale", analy->show_colorscale );
    cJSON_AddBoolToObject( toggles, "scale",  analy->show_scale );
    cJSON_AddBoolToObject( toggles, "date",   analy->show_datetime );
    cJSON_AddBoolToObject( toggles, "tinfo",  analy->show_tinfo );
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
 * node / element metadata queries (06 §6.1 MVP).
 *
 * Both resolve an "id" (user-facing label, 1-based or labels-aliased)
 * to the internal 0-based index and emit a populated `data` object
 * matching 06 §6.1. Integration-point arrays, attached-element lists,
 * and displacement are deferred past MVP per 06 §11 and reported as
 * `null` here so the response shape stays stable.
 * ------------------------------------------------------------------ */

/* Per-superclass connectivity stride. Matches the per-element node
 * counts draw.c uses; returning 0 for non-element superclasses lets
 * build_q_element skip connectivity output for those cases without
 * crashing. */
static int
connectivity_stride_for_superclass( int superclass )
{
    switch ( superclass )
    {
        case G_HEX:      return 8;
        case G_TET:      return 4;
        case G_QUAD:     return 4;
        case G_TRI:      return 3;
        case G_BEAM:     return 3;
        case G_TRUSS:    return 2;
        case G_PYRAMID:  return 5;
        case G_WEDGE:    return 6;
        case G_PARTICLE: return 1;
        default:         return 0;
    }
}

/* Invert a labels-table lookup: find the internal 0-based index whose
 * user-facing label equals `want`. Linear scan — fine for MVP, and
 * matches the current cost model (picks happen at human click rate).
 * Falls back to (want - 1) when the class has no labels table. */
static int
index_from_label( MO_class_data *p_mo_class, int want )
{
    int i;

    if ( p_mo_class == NULL )
        return -1;

    if ( !p_mo_class->labels_found || p_mo_class->labels == NULL )
    {
        int idx = want - 1;
        if ( idx < 0 || idx >= p_mo_class->qty )
            return -1;
        return idx;
    }

    for ( i = 0; i < p_mo_class->qty; i++ )
        if ( p_mo_class->labels[i].label_num == want )
            return p_mo_class->labels[i].local_id;

    return -1;
}

static cJSON *
build_q_node( Analysis *analy, int id )
{
    cJSON         *data;
    MO_class_data *p_mo_class;
    int            idx;

    if ( analy == NULL || analy->mesh_table == NULL || analy->mesh_qty <= 0 )
        return NULL;

    p_mo_class = analy->mesh_table[0].node_geom;
    if ( p_mo_class == NULL )
        return NULL;

    idx = index_from_label( p_mo_class, id );
    if ( idx < 0 )
        return NULL;

    data = cJSON_CreateObject();
    cJSON_AddStringToObject( data, "kind",  "node" );
    cJSON_AddNumberToObject( data, "id",    id );
    cJSON_AddNumberToObject( data, "index", idx + 1 );

    if ( analy->dimension == 3
         && analy->state_p != NULL
         && analy->state_p->nodes.nodes3d != NULL )
    {
        GVec3D *c = analy->state_p->nodes.nodes3d;
        cJSON  *a = cJSON_CreateArray();
        cJSON_AddItemToArray( a, cJSON_CreateNumber( c[idx][0] ) );
        cJSON_AddItemToArray( a, cJSON_CreateNumber( c[idx][1] ) );
        cJSON_AddItemToArray( a, cJSON_CreateNumber( c[idx][2] ) );
        cJSON_AddItemToObject( data, "coords", a );
    }
    else if ( analy->state_p != NULL
              && analy->state_p->nodes.nodes2d != NULL )
    {
        GVec2D *c = analy->state_p->nodes.nodes2d;
        cJSON  *a = cJSON_CreateArray();
        cJSON_AddItemToArray( a, cJSON_CreateNumber( c[idx][0] ) );
        cJSON_AddItemToArray( a, cJSON_CreateNumber( c[idx][1] ) );
        cJSON_AddItemToObject( data, "coords", a );
    }
    else
    {
        cJSON_AddNullToObject( data, "coords" );
    }

    /* Displacement + attached_elements are post-MVP (06 §11). */
    cJSON_AddNullToObject( data, "displacement" );
    cJSON_AddNullToObject( data, "attached_elements" );

    if ( p_mo_class->data_buffer != NULL && analy->cur_result != NULL )
        cJSON_AddNumberToObject( data, "result_value",
                                 p_mo_class->data_buffer[idx] );
    else
        cJSON_AddNullToObject(   data, "result_value" );

    return data;
}

void *
build_q_node_data( Analysis *analy, int id )
{
    return build_q_node( analy, id );
}

/* Pick the first element class whose label space covers `id`. Scans
 * superclasses in the order we instrument them in draw.c (hex, tet,
 * quad, tri, beam, truss, pyramid, wedge, particle) so the lookup
 * rule matches what pick_at resolves. Multi-class meshes fall back
 * to the first class with the id in range — 06 §11 deferral. */
static MO_class_data *
find_element_class_for_id( Analysis *analy, int id, int *out_index )
{
    static const int order[] = {
        G_HEX, G_TET, G_QUAD, G_TRI,
        G_BEAM, G_TRUSS, G_PYRAMID, G_WEDGE, G_PARTICLE
    };
    Mesh_data *mesh;
    size_t     i;
    int        k;

    if ( analy == NULL || analy->mesh_table == NULL
         || analy->mesh_qty <= 0 )
        return NULL;
    mesh = &analy->mesh_table[0];

    for ( i = 0; i < sizeof( order ) / sizeof( order[0] ); i++ )
    {
        int        sc = order[i];
        List_head *lh;
        if ( sc < 0 || sc >= QTY_SCLASS )
            continue;
        lh = &mesh->classes_by_sclass[sc];
        for ( k = 0; k < lh->qty; k++ )
        {
            MO_class_data *cd = ((MO_class_data **) lh->list)[k];
            int            idx;
            if ( cd == NULL )
                continue;
            idx = index_from_label( cd, id );
            if ( idx >= 0 )
            {
                if ( out_index != NULL )
                    *out_index = idx;
                return cd;
            }
        }
    }
    return NULL;
}

static cJSON *
build_q_element( Analysis *analy, int id )
{
    cJSON         *data;
    MO_class_data *p_mo_class;
    int            idx = -1;
    int            stride;

    p_mo_class = find_element_class_for_id( analy, id, &idx );
    if ( p_mo_class == NULL || idx < 0 )
        return NULL;

    data   = cJSON_CreateObject();
    stride = connectivity_stride_for_superclass( p_mo_class->superclass );

    cJSON_AddStringToObject( data, "kind",
        ( p_mo_class->short_name != NULL && p_mo_class->short_name[0] != '\0' )
            ? p_mo_class->short_name : "element" );
    cJSON_AddStringToObject( data, "type",
        ( p_mo_class->short_name != NULL && p_mo_class->short_name[0] != '\0' )
            ? p_mo_class->short_name : "element" );
    cJSON_AddNumberToObject( data, "id",    id );
    cJSON_AddNumberToObject( data, "index", idx + 1 );

    if ( p_mo_class->objects.elems != NULL
         && p_mo_class->objects.elems->mat != NULL )
        cJSON_AddNumberToObject( data, "material",
            p_mo_class->objects.elems->mat[idx] + 1 );
    else
        cJSON_AddNullToObject(   data, "material" );

    if ( stride > 0
         && p_mo_class->objects.elems != NULL
         && p_mo_class->objects.elems->nodes != NULL )
    {
        cJSON *conn = cJSON_CreateArray();
        int   *base = p_mo_class->objects.elems->nodes + (size_t) idx * stride;
        int    j;
        for ( j = 0; j < stride; j++ )
            cJSON_AddItemToArray( conn, cJSON_CreateNumber( base[j] + 1 ) );
        cJSON_AddItemToObject( data, "connectivity", conn );
    }
    else
    {
        cJSON_AddNullToObject( data, "connectivity" );
    }

    if ( p_mo_class->data_buffer != NULL && analy->cur_result != NULL )
        cJSON_AddNumberToObject( data, "result_value",
                                 p_mo_class->data_buffer[idx] );
    else
        cJSON_AddNullToObject(   data, "result_value" );

    /* Per-integration-point array is post-MVP (06 §11). */
    cJSON_AddNullToObject( data, "result_value_per_int_pt" );

    return data;
}

void *
build_q_element_data( Analysis *analy, int id )
{
    return build_q_element( analy, id );
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
/* q_node / q_element accept a numeric argument after the keyword. Return
 * 0 if the command doesn't match `prefix`; set *arg_out to the parsed
 * integer and return 1 on a clean parse; set *arg_out=-1 and return 1
 * if the prefix matched but the arg was missing / non-numeric (caller
 * surfaces invalid_syntax). */
static int
parse_numeric_arg( const char *cmd, const char *prefix, int *arg_out )
{
    size_t plen = strlen( prefix );
    const char *p;
    int         n   = 0;
    int         any = 0;

    if ( strncmp( cmd, prefix, plen ) != 0 )
        return 0;
    if ( cmd[plen] != '\0' && cmd[plen] != ' ' && cmd[plen] != '\t' )
        return 0;

    p = server_skip_ws( cmd + plen );
    if ( *p == '\0' )
    {
        *arg_out = -1;
        return 1;
    }
    while ( *p >= '0' && *p <= '9' )
    {
        n = n * 10 + ( *p - '0' );
        any = 1;
        p++;
    }
    if ( !any )
    {
        *arg_out = -1;
        return 1;
    }
    *arg_out = n;
    return 1;
}

int
server_try_query( const char *id, const char *cmd, Analysis *analy )
{
    const char *c;
    cJSON      *data = NULL;
    int         qid;

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
    else if ( parse_numeric_arg( c, "q_node", &qid ) )
    {
        if ( qid < 0 )
        {
            server_emit_error( id, "invalid_syntax",
                               "q_node requires a numeric id argument" );
            return 1;
        }
        data = build_q_node( analy, qid );
        if ( data == NULL )
        {
            server_emit_error( id, "not_found",
                               "no node with that id in the current mesh" );
            return 1;
        }
    }
    else if ( parse_numeric_arg( c, "q_element", &qid ) )
    {
        if ( qid < 0 )
        {
            server_emit_error( id, "invalid_syntax",
                               "q_element requires a numeric id argument" );
            return 1;
        }
        data = build_q_element( analy, qid );
        if ( data == NULL )
        {
            server_emit_error( id, "not_found",
                               "no element with that id in the current mesh" );
            return 1;
        }
    }
    else
        return 0;

    server_emit_data_response( id, data );
    return 1;
}

#endif /* GRIZ_SERVER_BUILD */
