/*
 * server_core.c - JSON envelope helpers for griz-server.
 *
 * See server_core.h for the request/response contract. This file is
 * linked only into the server binary (guarded by GRIZ_SERVER_BUILD)
 * so cJSON is not pulled into the batch/interactive binaries.
 */

#ifdef GRIZ_SERVER_BUILD

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "cJSON.h"
#include "server_core.h"
#include "server_events.h"
#include "server_query.h"
#include "server_render.h"

#define GRIZ_PROTOCOL_VERSION "1.0"
#define GRIZ_SERVER_MAX_LINE 4096

/* Heuristic: try JSON parsing only when the line looks like an object.
 * parse_command accepts lots of syntax, and feeding every raw command
 * through cJSON_Parse() would be wasteful (and would produce spurious
 * parse errors logged via cJSON_GetErrorPtr).
 */
static int
looks_like_json_object( const char *line )
{
    while ( *line == ' ' || *line == '\t' )
        line++;
    return *line == '{';
}

/* Default emitter: write the JSON text followed by a newline to stdout
 * and flush. Matches the stdio transport's line-delimited framing. */
static void
stdout_line_emitter( const char *buf, size_t len, void *ctx )
{
    (void) ctx;
    if ( buf == NULL || len == 0 )
        return;
    fwrite( buf, 1, len, stdout );
    fputc( '\n', stdout );
    fflush( stdout );
}

static ServerLineEmitter g_line_emitter     = stdout_line_emitter;
static void              *g_line_emitter_ctx = NULL;

/* Default binary-frame emitter: discard. The stdio transport does not
 * carry binary frames (02-protocol.md §6.1 — MCP stdio returns
 * screenshots as disk paths), and leaving a no-op default keeps callers
 * from having to special-case transport detection. */
static void
noop_binary_emitter( const unsigned char *payload, size_t len, void *ctx )
{
    (void) payload;
    (void) len;
    (void) ctx;
}

static ServerBinaryEmitter g_binary_emitter     = noop_binary_emitter;
static void               *g_binary_emitter_ctx = NULL;
/* 16 MiB frame cap (02-protocol.md §2.2). Defined separately here so
 * server_emit_binary_frame() can refuse oversized payloads before the
 * transport ever sees them. */
#define SERVER_BINARY_FRAME_MAX_PAYLOAD (16 * 1024 * 1024)

void
server_set_line_emitter( ServerLineEmitter fn, void *ctx )
{
    if ( fn == NULL )
    {
        g_line_emitter     = stdout_line_emitter;
        g_line_emitter_ctx = NULL;
    }
    else
    {
        g_line_emitter     = fn;
        g_line_emitter_ctx = ctx;
    }
}

ServerLineEmitter
server_current_line_emitter( void **ctx_out )
{
    if ( ctx_out != NULL )
        *ctx_out = g_line_emitter_ctx;
    return g_line_emitter;
}

void
server_emit_raw( const char *json_text )
{
    if ( json_text == NULL )
        return;
    g_line_emitter( json_text, strlen( json_text ), g_line_emitter_ctx );
}

void
server_set_binary_emitter( ServerBinaryEmitter fn, void *ctx )
{
    if ( fn == NULL )
    {
        g_binary_emitter     = noop_binary_emitter;
        g_binary_emitter_ctx = NULL;
    }
    else
    {
        g_binary_emitter     = fn;
        g_binary_emitter_ctx = ctx;
    }
}

int
server_has_binary_transport( void )
{
    return g_binary_emitter != noop_binary_emitter;
}

int
server_emit_binary_frame( unsigned char       subtype,
                          unsigned char       codec,
                          unsigned char       flags,
                          const char         *header_json,
                          const unsigned char *body,
                          size_t              body_len )
{
    size_t         hdr_len;
    size_t         payload_len;
    unsigned char *payload;
    unsigned char *wp;

    if ( g_binary_emitter == noop_binary_emitter )
        return -1;   /* transport does not carry binary frames */

    hdr_len = ( header_json != NULL ) ? strlen( header_json ) : 0;
    /* JSON sub-header length field is uint16: cap matches the
     * protocol (2-byte field precedes the JSON text, per §3). */
    if ( hdr_len > 0xFFFFu )
        return -1;

    /* 4 bytes control (subtype/codec/flags/reserved) + 2 bytes jhlen
     * + JSON header + body. Check against the transport cap before
     * allocating. */
    if ( body_len + hdr_len + 6 > SERVER_BINARY_FRAME_MAX_PAYLOAD )
        return -1;

    payload_len = 4 + 2 + hdr_len + body_len;
    payload     = (unsigned char *) malloc( payload_len );
    if ( payload == NULL )
        return -1;

    wp = payload;
    *wp++ = subtype;
    *wp++ = codec;
    *wp++ = flags;
    *wp++ = 0x00;                         /* reserved byte 3 */
    *wp++ = (unsigned char) ( ( hdr_len >> 8 ) & 0xFF );
    *wp++ = (unsigned char) (   hdr_len        & 0xFF );
    if ( hdr_len > 0 )
    {
        memcpy( wp, header_json, hdr_len );
        wp += hdr_len;
    }
    if ( body_len > 0 && body != NULL )
    {
        memcpy( wp, body, body_len );
    }

    g_binary_emitter( payload, payload_len, g_binary_emitter_ctx );
    free( payload );
    return 0;
}

static void
emit_json_line( cJSON *obj )
{
    char *rendered = cJSON_PrintUnformatted( obj );
    if ( rendered != NULL )
    {
        g_line_emitter( rendered, strlen( rendered ), g_line_emitter_ctx );
        free( rendered );
    }
}

/* Parse the major-version component of a semver-ish string. Returns
 * -1 on malformed input, else the leading integer. "1.0", "1", "1.2.3"
 * all parse to 1. */
static int
parse_major_version( const char *v )
{
    int major = 0;
    int any_digit = 0;

    if ( v == NULL )
        return -1;
    while ( *v >= '0' && *v <= '9' )
    {
        major = major * 10 + ( *v - '0' );
        any_digit = 1;
        v++;
    }
    if ( !any_digit )
        return -1;
    if ( *v != '\0' && *v != '.' )
        return -1;
    return major;
}

int
server_parse_request( const char *line, ServerRequest *req )
{
    cJSON *root;
    cJSON *cmd_item;
    cJSON *id_item;

    req->cmd  = NULL;
    req->id   = NULL;
    req->json = NULL;

    if ( !looks_like_json_object( line ) )
    {
        req->cmd = line;
        return 0;
    }

    root = cJSON_Parse( line );
    if ( root == NULL )
    {
        server_emit_error( NULL, "invalid_request",
                           "malformed JSON request" );
        return -1;
    }

    cmd_item = cJSON_GetObjectItemCaseSensitive( root, "cmd" );
    if ( !cJSON_IsString( cmd_item ) || cmd_item->valuestring == NULL )
    {
        id_item = cJSON_GetObjectItemCaseSensitive( root, "id" );
        server_emit_error(
            cJSON_IsString( id_item ) ? id_item->valuestring : NULL,
            "invalid_request",
            "request missing string 'cmd' field" );
        cJSON_Delete( root );
        return -1;
    }

    id_item = cJSON_GetObjectItemCaseSensitive( root, "id" );

    req->cmd  = cmd_item->valuestring;
    req->id   = ( cJSON_IsString( id_item ) ) ? id_item->valuestring : NULL;
    req->json = root;
    return 0;
}

void
server_request_free( ServerRequest *req )
{
    if ( req == NULL || req->json == NULL )
        return;
    cJSON_Delete( (cJSON *) req->json );
    req->json = NULL;
    req->cmd  = NULL;
    req->id   = NULL;
}

static void
add_id_field( cJSON *obj, const char *id )
{
    if ( id != NULL )
        cJSON_AddStringToObject( obj, "id", id );
    else
        cJSON_AddNullToObject( obj, "id" );
}

void
server_emit_response( const char *id, int status_ok,
                      const char *stdout_s, const char *stderr_s )
{
    cJSON *root;

    root = cJSON_CreateObject();
    if ( root == NULL )
        return;

    cJSON_AddStringToObject( root, "type",   "response" );
    add_id_field( root, id );
    cJSON_AddStringToObject( root, "status", status_ok ? "ok" : "error" );
    cJSON_AddStringToObject( root, "stdout", stdout_s ? stdout_s : "" );
    cJSON_AddStringToObject( root, "stderr", stderr_s ? stderr_s : "" );
    cJSON_AddNullToObject(   root, "data" );

    emit_json_line( root );
    cJSON_Delete( root );
}

void
server_emit_data_response( const char *id, void *data )
{
    cJSON *root;
    cJSON *data_obj;

    root = cJSON_CreateObject();
    if ( root == NULL )
    {
        if ( data != NULL ) cJSON_Delete( (cJSON *) data );
        return;
    }

    cJSON_AddStringToObject( root, "type",   "response" );
    add_id_field( root, id );
    cJSON_AddStringToObject( root, "status", "ok" );
    cJSON_AddStringToObject( root, "stdout", "" );
    cJSON_AddStringToObject( root, "stderr", "" );

    data_obj = ( data != NULL )
               ? (cJSON *) data
               : cJSON_CreateObject();
    cJSON_AddItemToObject( root, "data", data_obj );

    emit_json_line( root );
    cJSON_Delete( root );
}

void
server_emit_error( const char *id, const char *code, const char *message )
{
    cJSON *root;
    cJSON *err;

    root = cJSON_CreateObject();
    if ( root == NULL )
        return;

    cJSON_AddStringToObject( root, "type",   "response" );
    add_id_field( root, id );
    cJSON_AddStringToObject( root, "status", "error" );

    err = cJSON_CreateObject();
    cJSON_AddStringToObject( err, "code",    code    ? code    : "internal_error" );
    cJSON_AddStringToObject( err, "message", message ? message : "" );
    cJSON_AddItemToObject( root, "error", err );

    emit_json_line( root );
    cJSON_Delete( root );
}

/* Error-capture state. See server_core.h for the contract. Single
 * command at a time → single slot, no need for a ring or mutex.
 * `armed` is 1 between server_clear_error() and the next call, so a
 * stray popup_dialog from startup code (before we enter the command
 * loop, or from the handshake path) can't poison a future request. */
#define SERVER_ERROR_BUF 512
static int  server_error_armed   = 0;
static int  server_error_captured = 0;
static char server_error_message[SERVER_ERROR_BUF];
static char server_error_code[64];

void
server_clear_error( void )
{
    server_error_armed    = 1;
    server_error_captured = 0;
    server_error_message[0] = '\0';
    server_error_code[0]    = '\0';
}

/* INFO_POPUP covers both notices ("Already at state 1") and error
 * messages ("Command X not valid"). Capture only when the message
 * looks like a real error — otherwise commands that merely announce
 * a benign state would be mis-classified as failures.
 */
static int
info_message_is_error( const char *msg )
{
    if ( msg == NULL )
        return 0;
    return ( strstr( msg, "not valid"  ) != NULL
          || strstr( msg, "Invalid"    ) != NULL
          || strstr( msg, "invalid"    ) != NULL
          || strstr( msg, "Unknown"    ) != NULL
          || strstr( msg, "unknown"    ) != NULL
          || strstr( msg, "Bad "       ) != NULL
          || strstr( msg, "Usage:"     ) != NULL
          || strstr( msg, "not found"  ) != NULL
          || strstr( msg, "must "      ) != NULL );
}

void
server_record_error( int severity, const char *message )
{
    const char *code;

    if ( !server_error_armed || server_error_captured )
        return;

    /* Popup severities from viewerS.h — duplicated as literals here to
     * avoid dragging viewerS.h into server_core.c. INFO_POPUP=0,
     * USAGE_POPUP=1, WARNING_POPUP=2. */
    switch ( severity )
    {
        case 1:  /* USAGE_POPUP  — always an error */
            code = "invalid_syntax";
            break;
        case 2:  /* WARNING_POPUP — always an error */
            code = "command_error";
            break;
        case 0:  /* INFO_POPUP   — only capture if the text looks like one */
        default:
            if ( !info_message_is_error( message ) )
                return;
            /* Distinguish "not valid" → unknown_command from the rest. */
            if ( message != NULL && strstr( message, "not valid" ) != NULL )
                code = "unknown_command";
            else
                code = "command_error";
            break;
    }

    strncpy( server_error_code, code, sizeof( server_error_code ) - 1 );
    server_error_code[sizeof( server_error_code ) - 1] = '\0';

    if ( message != NULL )
    {
        strncpy( server_error_message, message,
                 sizeof( server_error_message ) - 1 );
        server_error_message[sizeof( server_error_message ) - 1] = '\0';
    }

    server_error_captured = 1;
}

int
server_peek_error( const char **code, const char **message )
{
    if ( !server_error_captured )
        return 0;
    if ( code    != NULL ) *code    = server_error_code;
    if ( message != NULL ) *message = server_error_message;
    return 1;
}

/* ---------------------------------------------------------------- *
 * Output capture — fd-level stdout/stderr redirect around each
 * command. See server_core.h for the contract.
 * ---------------------------------------------------------------- */

#define SERVER_CAPTURE_MAX (256 * 1024)  /* 256KB cap per stream */
static const char SERVER_CAPTURE_TRUNC_MSG[] = "…[truncated]\n";

static FILE *g_capture_stdout = NULL;
static FILE *g_capture_stderr = NULL;
static int   g_saved_stdout_fd = -1;
static int   g_saved_stderr_fd = -1;

static char *
read_captured_stream( FILE *fp )
{
    long   size;
    long   read_size;
    char  *buf;
    int    truncated = 0;
    size_t n;

    if ( fp == NULL )
        return NULL;

    fflush( fp );
    if ( fseek( fp, 0, SEEK_END ) != 0 )
        return strdup( "" );
    size = ftell( fp );
    if ( size < 0 )
        return strdup( "" );
    if ( fseek( fp, 0, SEEK_SET ) != 0 )
        return strdup( "" );

    read_size = size;
    if ( read_size > SERVER_CAPTURE_MAX )
    {
        read_size = SERVER_CAPTURE_MAX;
        truncated = 1;
    }

    buf = malloc( (size_t) read_size + sizeof( SERVER_CAPTURE_TRUNC_MSG ) );
    if ( buf == NULL )
        return strdup( "" );

    n = fread( buf, 1, (size_t) read_size, fp );
    if ( truncated )
    {
        memcpy( buf + n, SERVER_CAPTURE_TRUNC_MSG,
                sizeof( SERVER_CAPTURE_TRUNC_MSG ) );
    }
    else
    {
        buf[n] = '\0';
    }
    return buf;
}

void
server_capture_begin( void )
{
    if ( g_capture_stdout != NULL || g_capture_stderr != NULL )
        return;  /* already capturing; nested begin is a no-op */

    /* Flush any pending buffered output so the pre-capture bytes don't
     * bleed into the captured file. */
    fflush( stdout );
    fflush( stderr );

    g_saved_stdout_fd = dup( STDOUT_FILENO );
    g_saved_stderr_fd = dup( STDERR_FILENO );

    g_capture_stdout = tmpfile();
    g_capture_stderr = tmpfile();

    if ( g_capture_stdout == NULL || g_capture_stderr == NULL
         || g_saved_stdout_fd < 0 || g_saved_stderr_fd < 0 )
    {
        /* Best-effort: give up on capture, restore what we can. */
        if ( g_saved_stdout_fd >= 0 ) close( g_saved_stdout_fd );
        if ( g_saved_stderr_fd >= 0 ) close( g_saved_stderr_fd );
        if ( g_capture_stdout ) fclose( g_capture_stdout );
        if ( g_capture_stderr ) fclose( g_capture_stderr );
        g_capture_stdout = NULL;
        g_capture_stderr = NULL;
        g_saved_stdout_fd = -1;
        g_saved_stderr_fd = -1;
        return;
    }

    dup2( fileno( g_capture_stdout ), STDOUT_FILENO );
    dup2( fileno( g_capture_stderr ), STDERR_FILENO );

    /* Line-buffer the redirected streams so writes flush to the
     * tempfile promptly and we don't have to worry about leftover
     * buffered content sitting in the FILE* when we restore. */
    setvbuf( stdout, NULL, _IOLBF, 0 );
    setvbuf( stderr, NULL, _IONBF, 0 );
}

void
server_capture_end( char **stdout_out, char **stderr_out )
{
    char *out_s = NULL;
    char *err_s = NULL;

    if ( g_capture_stdout == NULL || g_capture_stderr == NULL )
    {
        if ( stdout_out != NULL ) *stdout_out = strdup( "" );
        if ( stderr_out != NULL ) *stderr_out = strdup( "" );
        return;
    }

    fflush( stdout );
    fflush( stderr );

    out_s = read_captured_stream( g_capture_stdout );
    err_s = read_captured_stream( g_capture_stderr );

    /* Restore the process-level stdout/stderr file descriptors. */
    dup2( g_saved_stdout_fd, STDOUT_FILENO );
    dup2( g_saved_stderr_fd, STDERR_FILENO );
    close( g_saved_stdout_fd );
    close( g_saved_stderr_fd );

    fclose( g_capture_stdout );
    fclose( g_capture_stderr );

    g_capture_stdout = NULL;
    g_capture_stderr = NULL;
    g_saved_stdout_fd = -1;
    g_saved_stderr_fd = -1;

    /* Return to sane buffering on the real stdout/stderr. */
    setvbuf( stdout, NULL, _IOLBF, 0 );
    setvbuf( stderr, NULL, _IONBF, 0 );

    if ( stdout_out != NULL ) *stdout_out = out_s != NULL ? out_s : strdup( "" );
    else                      free( out_s );
    if ( stderr_out != NULL ) *stderr_out = err_s != NULL ? err_s : strdup( "" );
    else                      free( err_s );
}

void
server_emit_ready( void )
{
    cJSON *root = cJSON_CreateObject();
    if ( root == NULL )
        return;

    cJSON_AddStringToObject( root, "type",    "event" );
    cJSON_AddStringToObject( root, "event",   "ready" );
    cJSON_AddStringToObject( root, "version", GRIZ_PROTOCOL_VERSION );
    cJSON_AddStringToObject( root, "server",  "griz-server" );

    emit_json_line( root );
    cJSON_Delete( root );
}

int
server_try_hello( const char *line )
{
    cJSON      *root;
    cJSON      *type_item;
    cJSON      *ver_item;
    cJSON      *ack;
    const char *ver_str;
    int         client_major;
    int         server_major;
    int         compatible;

    if ( !looks_like_json_object( line ) )
        return 0;

    root = cJSON_Parse( line );
    if ( root == NULL )
        return 0;

    type_item = cJSON_GetObjectItemCaseSensitive( root, "type" );
    if ( !cJSON_IsString( type_item ) || type_item->valuestring == NULL
         || strcmp( type_item->valuestring, "hello" ) != 0 )
    {
        cJSON_Delete( root );
        return 0;
    }

    ver_item = cJSON_GetObjectItemCaseSensitive( root, "version" );
    ver_str  = cJSON_IsString( ver_item ) ? ver_item->valuestring : NULL;
    client_major = parse_major_version( ver_str );
    server_major = parse_major_version( GRIZ_PROTOCOL_VERSION );
    compatible   = ( client_major >= 0 && client_major == server_major );

    ack = cJSON_CreateObject();
    if ( ack != NULL )
    {
        cJSON_AddStringToObject( ack, "type",           "hello_ack" );
        cJSON_AddStringToObject( ack, "version",        GRIZ_PROTOCOL_VERSION );
        cJSON_AddStringToObject( ack, "schema_version", GRIZ_PROTOCOL_VERSION );
        cJSON_AddStringToObject( ack, "server",         "griz-server" );
        cJSON_AddBoolToObject(   ack, "compatible",     compatible );
        if ( !compatible )
        {
            char buf[160];
            snprintf( buf, sizeof( buf ),
                      "incompatible protocol version: client=%s server=%s",
                      ver_str ? ver_str : "(none)", GRIZ_PROTOCOL_VERSION );
            cJSON_AddStringToObject( ack, "message", buf );
        }
        emit_json_line( ack );
        cJSON_Delete( ack );
    }

    cJSON_Delete( root );
    return 1;
}

/* ---------------------------------------------------------------- *
 * Transport-neutral per-line dispatcher. Lifted out of the stdio
 * loop so server_rpc.c (Phase 2 of planning/UI.md) can drive the
 * same state machine over length-framed JSON frames.
 * ---------------------------------------------------------------- */

static int
server_is_terminator( const char *s )
{
    return ( strcmp( s, "quit" ) == 0
          || strcmp( s, "exit" ) == 0
          || strcmp( s, "end"  ) == 0 );
}

/* Inline-screenshot command (05-rendering-and-streaming.md §7.1).
 * Emits a kind=0x02 binary frame carrying the PNG body, then a normal
 * JSON response with {seq, w, h, bytes}. Rejects on transports that
 * can't carry binary frames (stdio) with a typed error.
 *
 * Accepts both the bare command `screenshot` and JSON request forms:
 *   {"cmd":"screenshot"}
 *   {"cmd":"screenshot","alpha":true}
 *
 * Returns 1 if the command was handled (and the caller should skip
 * query dispatch + parse_command), 0 otherwise.
 */
static int
server_try_screenshot( ServerRequest *req, Analysis *analy )
{
    int            alpha = 0;
    unsigned char *rgba  = NULL;
    int            w     = 0;
    int            h     = 0;
    unsigned char *png   = NULL;
    size_t         png_len = 0;
    unsigned long long seq;

    if ( req == NULL || req->cmd == NULL )
        return 0;
    if ( strncmp( req->cmd, "screenshot", 10 ) != 0 )
        return 0;
    /* Accept exact match and the "screenshot ..." space-separated form. */
    if ( req->cmd[10] != '\0' && req->cmd[10] != ' ' && req->cmd[10] != '\t' )
        return 0;

    if ( !server_has_binary_transport() )
    {
        server_emit_error( req->id, "unsupported_command",
                           "screenshot requires a binary-capable transport"
                           " (use outrgb/outpng for disk output on stdio)" );
        return 1;
    }

    /* Optional alpha flag from the JSON request body. */
    if ( req->json != NULL )
    {
        cJSON *alpha_item = cJSON_GetObjectItemCaseSensitive(
            (cJSON *) req->json, "alpha" );
        if ( cJSON_IsBool( alpha_item ) )
            alpha = cJSON_IsTrue( alpha_item ) ? 1 : 0;
    }

    if ( server_render_capture_rgba( analy, &rgba, &w, &h ) != 0
         || rgba == NULL )
    {
        server_emit_error( req->id, "internal_error",
                           "failed to capture offscreen framebuffer" );
        return 1;
    }

    if ( server_render_encode_png( rgba, w, h, alpha,
                                   &png, &png_len ) != 0
         || png == NULL )
    {
        free( rgba );
        server_emit_error( req->id, "internal_error",
                           "PNG encode failed" );
        return 1;
    }
    free( rgba );

    seq = server_render_next_frame_seq();

    /* Build the JSON sub-header describing the body. The client uses
     * `request_id` to correlate the binary frame with the pending
     * request. */
    {
        cJSON *hdr = cJSON_CreateObject();
        char  *hdr_txt = NULL;
        if ( req->id != NULL )
            cJSON_AddStringToObject( hdr, "request_id", req->id );
        cJSON_AddNumberToObject( hdr, "w",       (double) w );
        cJSON_AddNumberToObject( hdr, "h",       (double) h );
        cJSON_AddNumberToObject( hdr, "seq",     (double) seq );
        cJSON_AddStringToObject( hdr, "fmt",     "png" );
        cJSON_AddNumberToObject( hdr, "bytes",   (double) png_len );
        hdr_txt = cJSON_PrintUnformatted( hdr );
        cJSON_Delete( hdr );

        /* subtype=0x02 (screenshot), codec=0x02 (png), flags=0x04 (last). */
        (void) server_emit_binary_frame( 0x02, 0x02, 0x04,
                                         hdr_txt, png, png_len );
        free( hdr_txt );
    }
    free( png );

    {
        cJSON *data = cJSON_CreateObject();
        cJSON_AddNumberToObject( data, "seq",   (double) seq );
        cJSON_AddNumberToObject( data, "w",     (double) w );
        cJSON_AddNumberToObject( data, "h",     (double) h );
        cJSON_AddNumberToObject( data, "bytes", (double) png_len );
        cJSON_AddStringToObject( data, "fmt",   "png" );
        server_emit_data_response( req->id, data );
    }

    return 1;
}

/* Viewport resize (05-rendering-and-streaming.md §2.1). JSON shape:
 *   {"cmd":"resize","w":<int>,"h":<int>}
 * Both dimensions are required integers; we reject missing/non-numeric
 * fields as invalid_request and out-of-range (> 4096) as resource_limit.
 * On success we emit a state_changed follow-up through the normal
 * notify path + an auto-push JPEG frame so the client immediately sees
 * the new viewport size.
 */
static int
server_try_resize( ServerRequest *req, Analysis *analy )
{
    cJSON *w_item;
    cJSON *h_item;
    int    w;
    int    h;
    int    rc;

    if ( req == NULL || req->cmd == NULL )
        return 0;
    if ( strcmp( req->cmd, "resize" ) != 0 )
        return 0;

    if ( req->json == NULL )
    {
        server_emit_error( req->id, "invalid_request",
                           "resize requires a JSON request body with"
                           " integer 'w' and 'h' fields" );
        return 1;
    }

    w_item = cJSON_GetObjectItemCaseSensitive( (cJSON *) req->json, "w" );
    h_item = cJSON_GetObjectItemCaseSensitive( (cJSON *) req->json, "h" );
    if ( !cJSON_IsNumber( w_item ) || !cJSON_IsNumber( h_item ) )
    {
        server_emit_error( req->id, "invalid_request",
                           "resize requires integer 'w' and 'h' fields" );
        return 1;
    }
    w = (int) w_item->valuedouble;
    h = (int) h_item->valuedouble;

    rc = server_render_resize_viewport( w, h );
    if ( rc == 1 )
    {
        server_emit_error( req->id, "resource_limit",
                           "viewport dimensions out of range"
                           " (each axis must be 1..4096)" );
        return 1;
    }
    if ( rc != 0 )
    {
        server_emit_error( req->id, "internal_error",
                           "OSMesa context resize failed" );
        return 1;
    }

    {
        cJSON *data = cJSON_CreateObject();
        cJSON_AddNumberToObject( data, "w", (double) w );
        cJSON_AddNumberToObject( data, "h", (double) h );
        server_emit_data_response( req->id, data );
    }

    /* Resize dirties the view on every downstream query: publish a
     * state_changed event + push a fresh frame so the client sees the
     * new dimensions without having to round-trip another command. */
    notify_state_all();
    notify_state_flush( analy );
    (void) server_render_push_jpeg_frame( analy, 0 );

    return 1;
}

int
server_core_dispatch_line( const char *line, Analysis *analy )
{
    ServerRequest req;
    char cmd_buf[GRIZ_SERVER_MAX_LINE];

    if ( line == NULL )
        return 0;

    /* Skip empty lines and #-comments (mirrors the old stdio loop). */
    while ( *line == ' ' || *line == '\t' )
        line++;
    if ( line[0] == '\0' || line[0] == '#' )
        return 0;

    /* Optional handshake: consume hello frames and loop back. The
     * client may send zero, one, or more hellos. */
    if ( server_try_hello( line ) )
        return 0;

    if ( server_parse_request( line, &req ) != 0 )
    {
        /* Malformed JSON — error response already emitted. */
        return 0;
    }

    if ( server_is_terminator( req.cmd ) )
    {
        server_emit_response( req.id, 1, "", "" );
        server_request_free( &req );
        return 1;
    }

    /* Dispatch query commands (q_state, q_view, q_time, ...) directly.
     * They bypass parse_command and emit a response with a populated
     * `data` field. */
    if ( server_try_query( req.id, req.cmd, analy ) )
    {
        server_request_free( &req );
        return 0;
    }

    /* Inline screenshot: drives the render + PNG path and emits its own
     * binary frame + response envelope. Handled before parse_command so
     * we don't need a new interpret.c keyword and so the stdio-mode
     * guard lives next to the transport emitter check. */
    if ( server_try_screenshot( &req, analy ) )
    {
        server_request_free( &req );
        return 0;
    }

    /* Viewport resize. Handled before parse_command because it isn't a
     * griz interpreter command — it reshapes the OSMesa framebuffer
     * and GL viewport directly. */
    if ( server_try_resize( &req, analy ) )
    {
        server_request_free( &req );
        return 0;
    }

    /* parse_command() takes a mutable buffer (it tokenises in place);
     * copy the resolved command so the cJSON-owned string is not
     * disturbed. Truncate on overflow — GRIZ_SERVER_MAX_LINE matches
     * the stdio input line limit so truncation shouldn't happen in
     * practice unless the client sends a pathological JSON request. */
    strncpy( cmd_buf, req.cmd, sizeof( cmd_buf ) - 1 );
    cmd_buf[sizeof( cmd_buf ) - 1] = '\0';

    server_clear_error();
    server_capture_begin();
    notify_state_reset();
    parse_command( cmd_buf, analy );
    {
        char       *out_s = NULL;
        char       *err_s = NULL;
        const char *err_code    = NULL;
        const char *err_message = NULL;
        int         had_error;

        server_capture_end( &out_s, &err_s );

        had_error = server_peek_error( &err_code, &err_message );
        if ( had_error )
        {
            server_emit_error( req.id, err_code, err_message );
        }
        else
        {
            server_emit_response( req.id, 1,
                                  out_s ? out_s : "",
                                  err_s ? err_s : "" );
        }
        free( out_s );
        free( err_s );

        /* MVP: emit a state_changed event after every successful
         * mutating command. Instrumentation in interpret.c will
         * eventually replace this blanket flag with targeted
         * notify_state(key) calls; see planning/ui-design/
         * 03-server.md §5.1. */
        if ( !had_error )
        {
            notify_state_all();
            notify_state_flush( analy );

            /* Push an auto-rendered JPEG frame on any transport that
             * carries binary frames (RPC). Pure query commands return
             * early above, so this only fires for genuinely mutating
             * commands — the MVP proof-of-life path from
             * 05-rendering-and-streaming.md §9. No-op on stdio. */
            (void) server_render_push_jpeg_frame( analy, 0 );
        }
    }
    server_request_free( &req );
    return 0;
}

#endif /* GRIZ_SERVER_BUILD */
