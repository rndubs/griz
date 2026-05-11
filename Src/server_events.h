/*
 * server_events.h - state_changed emitter for griz-server.
 *
 * Implements the state_seq counter, dirty-bit set, and emission helper
 * described in planning/ui-design/03-server.md §5 and
 * planning/shared/command-protocol.md § State event.
 *
 * Model (Phase 1 MVP):
 *
 *   1. notify_state_reset()  — called before each command dispatch.
 *   2. interpret.c handlers that mutate state call notify_state(key)
 *      or notify_state_all() after the mutation succeeds. In the
 *      current refactor the stdio loop also calls notify_state_all()
 *      unconditionally after any non-query command, so the event
 *      stream works end-to-end even before the instrumentation sweep
 *      of interpret.c lands. Later phases will replace that blanket
 *      call with per-handler notify_state(key) invocations to reduce
 *      event volume.
 *   3. notify_state_flush(analy) — called after the response for the
 *      current request is emitted, but before the next request is
 *      dispatched (per the ordering rule in command-protocol.md
 *      § Pipelining).
 *
 * Keys are the top-level schema buckets from query-commands.md:
 *   "database" | "time" | "view" | "render" | "materials" | "results"
 *   | "selection"
 *
 * In non-server builds all entry points compile to (void)0 via
 * guards in this header, so interpret.c remains buildable under the
 * legacy GUI and batch configurations (invariant I8).
 */

#ifndef SERVER_EVENTS_H
#define SERVER_EVENTS_H

#ifdef GRIZ_SERVER_BUILD

#include "viewer.h"

/* Mark the named top-level state key as dirty. Safe to call multiple
 * times per command; only one state_changed event will fire on flush. */
void notify_state( const char *key );

/* Convenience: mark every top-level state key as dirty. Used by the
 * stdio dispatch loop as the Phase-1 shim until each handler in
 * interpret.c is instrumented with a targeted notify_state(key). */
void notify_state_all( void );

/* Reset dirty bits. Call before dispatching a new command. */
void notify_state_reset( void );

/* Emit a state_changed event if any keys are dirty. Returns the
 * updated state_seq (monotonic, starts at 1). If no keys are dirty,
 * returns the current counter unchanged and emits nothing. */
unsigned long long notify_state_flush( Analysis *analy );

/* Current state_seq (last emitted). Useful for ready-event payloads
 * and debugging. */
unsigned long long notify_state_seq( void );

#else /* !GRIZ_SERVER_BUILD */

#define notify_state( key )         ((void) 0)
#define notify_state_all()          ((void) 0)
#define notify_state_reset()        ((void) 0)
#define notify_state_flush( analy ) ((void) 0)
#define notify_state_seq()          ((unsigned long long) 0)

#endif /* GRIZ_SERVER_BUILD */

#endif /* SERVER_EVENTS_H */
