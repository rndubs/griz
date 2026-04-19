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

#define GRIZ_PROTOCOL_VERSION "1.0"

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

static void
emit_json_line( cJSON *obj )
{
    char *rendered = cJSON_PrintUnformatted( obj );
    if ( rendered != NULL )
    {
        fputs( rendered, stdout );
        fputc( '\n', stdout );
        fflush( stdout );
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

#endif /* GRIZ_SERVER_BUILD */
