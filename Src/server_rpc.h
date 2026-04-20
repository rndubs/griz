/*
 * server_rpc.h - Length-framed RPC transport for griz-server.
 *
 * Phase 2 of planning/UI.md. Implements the wire contract described in
 * planning/ui-design/02-protocol.md §2: a single-connection TCP
 * listener on 127.0.0.1:<ephemeral>, a 32-byte rendezvous token, a
 * 5-byte framing header (`uint32 N + kind`), and the JSON envelope
 * semantics shared with the stdio transport.
 *
 * The RPC loop is a strict lift of the stdio path:
 *
 *   bind → rendezvous → accept → hello+token → server_core_dispatch_line
 *
 * per planning/ui-design/02-protocol.md §7. All three-thread work
 * (command/render/I-O split) is deferred.
 */

#ifndef SERVER_RPC_H
#define SERVER_RPC_H

#ifdef GRIZ_SERVER_BUILD

/* Entry point for --transport=rpc. Blocks until the single connected
 * client disconnects or emits a terminator command (quit/exit/end).
 * Returns 0 on clean shutdown and non-zero on setup failure.
 *
 *   db_path        : Mili database path (same as the stdio transport).
 *   width, height  : optional explicit viewport size; 0 means default.
 *   bind_host      : NULL for default (127.0.0.1).
 *   bind_port      : 0 for kernel-assigned ephemeral (the norm).
 *   rendezvous_path: path at which to write the rendezvous JSON (0600).
 *                    NULL selects the default
 *                    $HOME/.griz/rendezvous/{session-id}.json.
 */
int process_server_mode_rpc( const char *db_path,
                             int width, int height,
                             const char *bind_host,
                             int bind_port,
                             const char *rendezvous_path );

#endif /* GRIZ_SERVER_BUILD */

#endif /* SERVER_RPC_H */
