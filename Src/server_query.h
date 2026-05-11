/*
 * server_query.h - Query-command dispatcher for griz-server.
 *
 * The q_* family of read-only commands bypass parse_command() and emit
 * a response with a populated `data` field directly. See
 * planning/shared/query-commands.md for the canonical schema.
 *
 * The builders (build_q_state_data, build_q_view_data, ...) live in
 * server_query.c. server_try_query() recognises any q_* command name
 * and emits the corresponding data response.
 */

#ifndef SERVER_QUERY_H
#define SERVER_QUERY_H

#ifdef GRIZ_SERVER_BUILD

#include "viewer.h"

/* If `cmd` is a recognised query command (q_state, q_view, q_time,
 * q_materials, q_results, q_selection, q_render, q_database), emit a
 * response with a populated `data` field and return 1. Otherwise
 * return 0 and leave the caller to dispatch through parse_command().
 */
int server_try_query( const char *id, const char *cmd, Analysis *analy );

/* Expose the builders so the state-event emitter (server_events.c) can
 * reuse them to assemble a fresh q_state snapshot on overflow. Each
 * returns a newly-allocated cJSON object; ownership transfers to the
 * caller. Typed as void* to avoid pulling cJSON.h into every caller. */
void *build_q_state_data(     Analysis *analy );
void *build_q_time_data(      Analysis *analy );
void *build_q_view_data(      Analysis *analy );
void *build_q_materials_data( Analysis *analy );
void *build_q_results_data(   Analysis *analy );
void *build_q_selection_data( Analysis *analy );
void *build_q_render_data(    Analysis *analy );
void *build_q_database_data(  Analysis *analy );

/* `id` is the user-facing label (1-based, or labels-table alias) —
 * matches what clients see from q_selection / pick_at responses. Both
 * return NULL when the id doesn't resolve to a node / element, so the
 * dispatcher can surface `not_found`. */
void *build_q_node_data(      Analysis *analy, int id );
void *build_q_element_data(   Analysis *analy, int id );

#endif /* GRIZ_SERVER_BUILD */

#endif /* SERVER_QUERY_H */
