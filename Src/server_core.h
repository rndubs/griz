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

/* Emit an error response with a structured `error` object.
 *
 *   id      - request id to echo (NULL → JSON null)
 *   code    - machine-readable error code (e.g. "invalid_request")
 *   message - human-readable description
 */
void server_emit_error( const char *id, const char *code,
                        const char *message );

#endif /* GRIZ_SERVER_BUILD */

#endif /* SERVER_CORE_H */
