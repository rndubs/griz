/*
 * server_core.h - JSON envelope helpers for griz-server.
 *
 * Phase 2 piece 1 of planning/MCP.md: request/response framing with
 * cJSON. Subsequent pieces (handshake, output capture, error taxonomy,
 * query commands) will layer onto these helpers.
 *
 * The server accepts either a JSON request object
 *     {"type":"request","id":"<id>","cmd":"<griz command>"}
 * or a bare raw command line (for interactive debugging, per
 * planning/mcp/05-protocol.md §7).
 *
 * Every executed command emits a single JSON response line of the form
 *     {"type":"response","id":<id-or-null>,"status":"ok",
 *      "stdout":"","stderr":"","data":null}
 * Until the Phase 2 output-capture item lands, `stdout` and `stderr`
 * are empty strings: command chatter still flows through the process's
 * real stdout and will intersperse with response lines. That is a
 * known, bounded gap — Python callers currently treat post-READY
 * stdout as opaque.
 */

#ifndef SERVER_CORE_H
#define SERVER_CORE_H

#ifdef GRIZ_SERVER_BUILD

#include <stddef.h>

#include "viewer.h"

/* Transport-neutral "emit a JSON envelope" hook. Every response, event,
 * and hello_ack routes through the installed emitter. The stdio
 * transport installs a default emitter that writes `buf` followed by a
 * newline to stdout; the RPC transport (Src/server_rpc.c) installs an
 * emitter that length-prefixes `buf` into a kind=0x01 frame on the
 * socket. `buf` is a UTF-8 JSON object, `len` is strlen(buf), and the
 * emitter must not hold onto `buf` across the call.
 */
typedef void (*ServerLineEmitter)( const char *buf, size_t len, void *ctx );

/* Install a new emitter. Passing NULL restores the built-in stdout
 * emitter. The previous emitter/ctx are not returned; callers that
 * need to restore should capture them via server_current_line_emitter()
 * before installing a replacement. */
void               server_set_line_emitter(     ServerLineEmitter fn, void *ctx );
ServerLineEmitter  server_current_line_emitter( void **ctx_out );

/* Emit a single framed JSON envelope through the current emitter. The
 * stdio emitter appends a newline; the RPC emitter prepends a 5-byte
 * framing header. Safe to call with NULL — does nothing. */
void               server_emit_raw(             const char *json_text );

/* Binary-frame emitter hook (02-protocol.md §2.2, §3). Writes the
 * payload of a `kind=0x02` frame. The payload already carries the
 * subtype/codec/flags header and optional JSON sub-header built by
 * server_emit_binary_frame(); the transport emitter is only
 * responsible for framing (length prefix on RPC, discard on stdio).
 */
typedef void (*ServerBinaryEmitter)( const unsigned char *payload,
                                     size_t len, void *ctx );

/* Install a binary-frame emitter. NULL restores the default (a silent
 * no-op — binary frames are not carried over stdio). */
void server_set_binary_emitter( ServerBinaryEmitter fn, void *ctx );

/* Query whether a non-default binary emitter is installed (i.e., the
 * transport can carry binary frames). Used by command handlers that
 * need to reject `screenshot` on stdio with a typed error. */
int  server_has_binary_transport( void );

/* Build a kind=0x02 frame payload and dispatch to the binary emitter.
 *
 *   subtype      : 0x01=frame, 0x02=screenshot, 0x03=pick_buffer, ...
 *   codec        : 0x00=raw, 0x01=jpeg, 0x02=png, 0x03=h264, ...
 *   flags        : bit0=continuation, bit1=keyframe, bit2=last.
 *   header_json  : optional UTF-8 JSON object describing the body
 *                  (e.g. `{"request_id":"abc","w":1024,"h":1024,"fmt":"png"}`);
 *                  may be NULL or empty.
 *   body         : codec-encoded body bytes; may be NULL if body_len=0.
 *   body_len     : length of `body` in bytes.
 *
 * Returns 0 on success, -1 on size-cap overflow, no installed emitter,
 * or allocation failure. The payload is at most
 *   4 + 2 + strlen(header_json) + body_len bytes
 * and must stay under the 16 MiB transport cap; callers that expect
 * larger bodies must chunk into continuation frames.
 */
int server_emit_binary_frame( unsigned char       subtype,
                              unsigned char       codec,
                              unsigned char       flags,
                              const char         *header_json,
                              const unsigned char *body,
                              size_t              body_len );

typedef struct {
    /* Command string to feed the interpreter. Points into either the
     * caller's raw line buffer or into an owning cJSON object held in
     * ServerRequest.json (if json != NULL). */
    const char *cmd;

    /* Request id echoed back in the response. NULL when the client sent
     * a raw (non-JSON) command. */
    const char *id;

    /* Owning cJSON root for the parsed request, or NULL. Must be freed
     * via server_request_free(). */
    void *json;
} ServerRequest;

/* Parse a single line of input into a ServerRequest.
 *
 * The line is consumed in place (not modified). On a valid JSON request
 * object with a string `cmd`, req->cmd/req->id point into req->json and
 * the caller must call server_request_free() after use. On a raw line,
 * req->cmd is `line` itself and req->json is NULL.
 *
 * Returns 0 on success, -1 if the input is JSON-shaped but malformed
 * or missing a usable `cmd` field (an error response has already been
 * emitted in that case).
 */
int server_parse_request( const char *line, ServerRequest *req );

/* Free owned state (the cJSON root, if any). Safe to call when
 * req->json is NULL. */
void server_request_free( ServerRequest *req );

/* Emit a standard response line to stdout.
 *
 *   id        - request id to echo (NULL → JSON null)
 *   status_ok - non-zero for status:"ok", zero for status:"error"
 *   stdout_s  - captured stdout string (may be NULL → empty)
 *   stderr_s  - captured stderr string (may be NULL → empty)
 *
 * Flushes stdout before returning.
 */
void server_emit_response( const char *id, int status_ok,
                           const char *stdout_s, const char *stderr_s );

/* Emit a successful query response carrying structured data.
 *
 *   data - opaque pointer to a cJSON object. Ownership transfers into
 *          the response envelope and is freed here; callers must not
 *          touch `data` after this call. Pass NULL to emit an empty
 *          object. Type is void* so callers outside of server_core
 *          don't need to pull in cJSON.h.
 */
void server_emit_data_response( const char *id, void *data );

/* Emit an error response with a structured `error` object.
 *
 *   id      - request id to echo (NULL → JSON null)
 *   code    - machine-readable error code (e.g. "invalid_request")
 *   message - human-readable description
 */
void server_emit_error( const char *id, const char *code,
                        const char *message );

/* Emit the startup ready event:
 *   {"type":"event","event":"ready","version":"<proto-version>"}
 * Clients block on this line before sending any request or hello.
 * Replaces the plain "READY" sentinel used during Phase 1.
 */
void server_emit_ready( void );

/* Output-capture API (05-protocol.md §4.1). Between a begin/end pair
 * the process's real stdout and stderr are redirected to temporary
 * files at the fd level, so every write() / printf() / wrt_text() /
 * fprintf(stderr,…) call during command execution is absorbed instead
 * of interleaving with JSON response frames.
 *
 * Usage:
 *     server_capture_begin();
 *     parse_command( cmd_buf, analy );
 *     char *out = NULL, *err = NULL;
 *     server_capture_end( &out, &err );
 *     server_emit_response( req.id, 1, out, err );
 *     free( out ); free( err );
 *
 * Captured output is truncated at SERVER_CAPTURE_MAX bytes per stream;
 * truncation is indicated by an appended "…[truncated]\n" sentinel.
 */
void server_capture_begin( void );
void server_capture_end( char **stdout_out, char **stderr_out );

/* Error-capture hook (05-protocol.md §5). popup_dialog() calls
 * server_record_error() when running in the server binary with no
 * GUI attached, so the command loop can translate interpreter
 * diagnostics into a structured error response rather than letting
 * them leak to stderr alone.
 *
 * severity: the popup_dialog dtype (INFO_POPUP=0, USAGE_POPUP=1,
 *           WARNING_POPUP=2). Used to pick an error code.
 * message:  already-formatted diagnostic. Copied into an internal
 *           buffer, so caller-side lifetime doesn't matter.
 *
 * Only the first diagnostic per command is retained; subsequent calls
 * are ignored. Behavior is a no-op outside of a command boundary
 * (i.e. if server_clear_error() has not been called for the current
 * request).
 */
void server_record_error( int severity, const char *message );

/* Reset the captured-error slot. Call immediately before dispatching
 * a command to parse_command(). */
void server_clear_error( void );

/* If an error was captured since the last server_clear_error(),
 * return 1 and populate *code / *message. Pointers are owned by
 * server_core and valid until the next server_clear_error(). */
int  server_peek_error( const char **code, const char **message );

/* Peek at a line and, if it is a protocol hello message, consume it
 * and emit the matching hello_ack (or an incompat error). Returns:
 *
 *    1  - line was a hello and the handshake reply was emitted.
 *         Caller should read the next line and try again (the client
 *         may send multiple hellos, though that is not expected).
 *    0  - line is not a hello. Caller should fall through and treat
 *         the line as a request.
 *
 * Version rule (05-protocol.md §3.2): client and server must share a
 * major version; minor/patch mismatches are accepted. A bad major
 * version still emits a hello_ack with a "compatible":false note so
 * the client can decide whether to abort.
 */
int server_try_hello( const char *line );

/* Iterate a result hash table (primal_results or derived_results) and
 * append {name, title, origin} cJSON objects to the array `arr`.
 * All pointers are void* to avoid pulling in cJSON.h / viewer.h from
 * callers. `ht` is a Hash_table*, `arr` is a cJSON array.
 * Returns `arr` for chaining. Defined in results.c. */
void *server_build_results_from_htable( void *arr, void *ht,
                                        const char *origin_label );

/* Transport-neutral one-line dispatcher. Consumes a single input line
 * (raw command or JSON envelope), parses it, drives the hello / query /
 * parse_command / state_changed pipeline, and emits the response and
 * any pending state_changed event through the installed emitter.
 *
 * Returns:
 *    0  — normal, caller should continue reading.
 *    1  — terminator command (quit/exit/end) observed; caller should
 *         drain and close the transport.
 *
 * All JSON-shape errors are reported inline via an error response and
 * still return 0 so the caller keeps reading.
 *
 * `line` is used in place: on a raw command it is handed straight to
 * parse_command; on a JSON request it is cJSON-parsed. parse_command
 * tokenises in place, so the dispatcher copies into a scratch buffer
 * first.
 */
int server_core_dispatch_line( const char *line, Analysis *analy );

#endif /* GRIZ_SERVER_BUILD */

#endif /* SERVER_CORE_H */
