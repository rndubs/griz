/*
 * server_events.c - state_changed emitter for griz-server.
 *
 * Owns the state_seq counter, a fixed-size dirty-bit set over the
 * top-level state schema keys, and the emission helper. See
 * planning/ui-design/03-server.md §5 for the design and the event
 * shape in planning/shared/command-protocol.md.
 *
 * Phase 1 MVP: single-threaded. The stdio dispatcher calls
 * notify_state_reset() before a command, notify_state_all() (or
 * targeted notify_state(key) calls from interpret.c) during it, and
 * notify_state_flush() after the response is emitted. No queue, no
 * overflow path yet — those land with the RPC three-thread model in
 * Phase 2 of planning/UI.md.
 */

#ifdef GRIZ_SERVER_BUILD

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "viewer.h"
#include "cJSON.h"
#include "server_core.h"
#include "server_events.h"
#include "server_query.h"

/* Schema keys. Order must match STATE_KEY_* indices below. */
static const char *const STATE_KEYS[] = {
    "database",
    "time",
    "view",
    "render",
    "materials",
    "results",
    "selection"
};
#define STATE_KEY_QTY ( (int) (sizeof(STATE_KEYS) / sizeof(STATE_KEYS[0])) )

static int s_dirty[STATE_KEY_QTY];
static unsigned long long s_state_seq = 0;

static int
key_index( const char *key )
{
    int i;
    for ( i = 0; i < STATE_KEY_QTY; i++ )
    {
        if ( strcmp( STATE_KEYS[i], key ) == 0 )
            return i;
    }
    return -1;
}

void
notify_state( const char *key )
{
    int idx;
    if ( key == NULL )
        return;
    idx = key_index( key );
    if ( idx >= 0 )
        s_dirty[idx] = 1;
}

void
notify_state_all( void )
{
    int i;
    for ( i = 0; i < STATE_KEY_QTY; i++ )
        s_dirty[i] = 1;
}

void
notify_state_reset( void )
{
    int i;
    for ( i = 0; i < STATE_KEY_QTY; i++ )
        s_dirty[i] = 0;
}

unsigned long long
notify_state_seq( void )
{
    return s_state_seq;
}

/* Dispatch to the right builder for the named key. Typed as void*
 * everywhere to avoid a cJSON dep in the public header. */
static cJSON *
build_for_key( Analysis *analy, const char *key )
{
    if ( strcmp( key, "database"  ) == 0 ) return build_q_database_data(  analy );
    if ( strcmp( key, "time"      ) == 0 ) return build_q_time_data(      analy );
    if ( strcmp( key, "view"      ) == 0 ) return build_q_view_data(      analy );
    if ( strcmp( key, "render"    ) == 0 ) return build_q_render_data(    analy );
    if ( strcmp( key, "materials" ) == 0 ) return build_q_materials_data( analy );
    if ( strcmp( key, "results"   ) == 0 ) return build_q_results_data(   analy );
    if ( strcmp( key, "selection" ) == 0 ) return build_q_selection_data( analy );
    return NULL;
}

/* Emit a single line:
 *   {"type":"event","event":"state_changed","state_seq":N,"fields":{...}}
 * Routes through the shared server_core emitter so stdio and RPC
 * transports share the same code path. */
static void
emit_state_changed( cJSON *fields )
{
    cJSON *root = cJSON_CreateObject();
    char  *txt;

    cJSON_AddStringToObject( root, "type",       "event" );
    cJSON_AddStringToObject( root, "event",      "state_changed" );
    cJSON_AddNumberToObject( root, "state_seq",  (double) s_state_seq );
    cJSON_AddItemToObject(   root, "fields",     fields );

    txt = cJSON_PrintUnformatted( root );
    if ( txt != NULL )
    {
        server_emit_raw( txt );
        free( txt );
    }
    cJSON_Delete( root );
}

unsigned long long
notify_state_flush( Analysis *analy )
{
    cJSON *fields;
    int any = 0;
    int i;

    if ( analy == NULL )
        return s_state_seq;

    for ( i = 0; i < STATE_KEY_QTY; i++ )
    {
        if ( s_dirty[i] )
        {
            any = 1;
            break;
        }
    }
    if ( !any )
        return s_state_seq;

    fields = cJSON_CreateObject();
    for ( i = 0; i < STATE_KEY_QTY; i++ )
    {
        if ( s_dirty[i] )
        {
            cJSON *sub = build_for_key( analy, STATE_KEYS[i] );
            if ( sub != NULL )
                cJSON_AddItemToObject( fields, STATE_KEYS[i], sub );
        }
    }

    s_state_seq++;
    emit_state_changed( fields );       /* takes ownership of `fields` */
    notify_state_reset();
    return s_state_seq;
}

#endif /* GRIZ_SERVER_BUILD */
