/*
 * server_rpc.c - Length-framed RPC transport for griz-server.
 *
 * See server_rpc.h for the entry point contract and planning/ui-design/
 * 02-protocol.md for the wire format.
 *
 * v0 shape: single-threaded, single-connection. The RPC loop hands
 * every decoded JSON frame straight to server_core_dispatch_line(),
 * which is the same dispatcher the stdio transport uses. Heartbeats
 * (kind=0x03) echo the client's 8-byte timestamp back so the client
 * can compute RTT. Binary kind (0x02) is not yet accepted from the
 * client; the render/frame-push pipeline (Phase 3) will drive it from
 * the server side.
 *
 * Post-MVP (tracked in planning/UI.md § Phase 2):
 *   - Three-thread split (command / render / I-O) with bounded queues.
 *   - 20 s heartbeat cadence and 60 s peer-idle detection.
 *   - SIGTERM handler with graceful flush.
 */

#ifdef GRIZ_SERVER_BUILD

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#include "viewer.h"
#include "cJSON.h"
#include "server_core.h"
#include "server_core_startup.h"
#include "server_events.h"
#include "server_rpc.h"

/* --- Framing constants (02-protocol.md §2.2). ---------------------- */
#define RPC_FRAME_KIND_JSON       0x01
#define RPC_FRAME_KIND_BINARY     0x02
#define RPC_FRAME_KIND_HEARTBEAT  0x03
#define RPC_FRAME_MAX_PAYLOAD     (16 * 1024 * 1024)   /* 16 MiB cap */
#define RPC_FRAME_HEADER_BYTES    5

#define RPC_TOKEN_BYTES           32
/* base64(32) = 44 chars (including trailing '='); +1 for NUL. */
#define RPC_TOKEN_B64_LEN         45

#define RPC_SESSION_ID_CHARS      8   /* "griz-XXXXXXXX" → 13 total */

/* --- Connection state --------------------------------------------- */

typedef struct {
    int  fd;                                /* connected client socket */
    char token_b64[RPC_TOKEN_B64_LEN];      /* rendezvous token, NUL-terminated */
} RpcContext;

/* Module-level singleton so the signal handler can reach the socket. */
static RpcContext g_rpc = { -1, { 0 } };
static volatile sig_atomic_t g_signal_term_pending = 0;

/* --- Base64 (standard alphabet, no line wrap) --------------------- */

static void
rpc_base64_encode( const unsigned char *src, size_t src_len, char *dst )
{
    static const char alpha[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t i = 0;
    size_t o = 0;

    while ( i + 3 <= src_len )
    {
        unsigned v = (unsigned) src[i]   << 16
                   | (unsigned) src[i+1] << 8
                   | (unsigned) src[i+2];
        dst[o++] = alpha[(v >> 18) & 0x3f];
        dst[o++] = alpha[(v >> 12) & 0x3f];
        dst[o++] = alpha[(v >>  6) & 0x3f];
        dst[o++] = alpha[ v        & 0x3f];
        i += 3;
    }
    if ( i < src_len )
    {
        unsigned v = (unsigned) src[i] << 16;
        int rem = 0;
        if ( i + 1 < src_len ) { v |= (unsigned) src[i+1] << 8; rem = 1; }
        dst[o++] = alpha[(v >> 18) & 0x3f];
        dst[o++] = alpha[(v >> 12) & 0x3f];
        dst[o++] = ( rem ) ? alpha[(v >> 6) & 0x3f] : '=';
        dst[o++] = '=';
    }
    dst[o] = '\0';
}

/* --- Socket I/O helpers ------------------------------------------- */

/* Read exactly `need` bytes from fd into buf. Returns `need` on
 * success, 0 on clean EOF before any bytes, -1 on truncated read or
 * hard error. */
static ssize_t
read_exact( int fd, void *buf, size_t need )
{
    unsigned char *p = (unsigned char *) buf;
    size_t got = 0;

    while ( got < need )
    {
        ssize_t n = read( fd, p + got, need - got );
        if ( n == 0 )
            return (got == 0) ? 0 : -1;
        if ( n < 0 )
        {
            if ( errno == EINTR )
                continue;
            return -1;
        }
        got += (size_t) n;
    }
    return (ssize_t) need;
}

/* Write exactly `len` bytes to fd from buf. Returns 0 on success, -1
 * on hard error or truncated write. */
static int
write_exact( int fd, const void *buf, size_t len )
{
    const unsigned char *p = (const unsigned char *) buf;
    size_t sent = 0;

    while ( sent < len )
    {
        ssize_t n = write( fd, p + sent, len - sent );
        if ( n < 0 )
        {
            if ( errno == EINTR )
                continue;
            return -1;
        }
        if ( n == 0 )
            return -1;
        sent += (size_t) n;
    }
    return 0;
}

/* Write one length-prefixed frame. Returns 0 on success. */
static int
rpc_write_frame( int fd, unsigned char kind, const void *payload, size_t len )
{
    unsigned char header[RPC_FRAME_HEADER_BYTES];
    uint32_t nlen;

    if ( len > RPC_FRAME_MAX_PAYLOAD )
        return -1;

    nlen = htonl( (uint32_t) len );
    memcpy( header, &nlen, 4 );
    header[4] = kind;

    if ( write_exact( fd, header, RPC_FRAME_HEADER_BYTES ) != 0 )
        return -1;
    if ( len > 0 && write_exact( fd, payload, len ) != 0 )
        return -1;
    return 0;
}

/* Read one frame. Allocates `*payload_out` (freed by caller) and
 * populates `*kind_out`, `*len_out`. Returns:
 *    0 = frame read ok
 *    1 = peer closed cleanly
 *   -1 = error (oversize, short read, protocol breakage)
 */
static int
rpc_read_frame( int fd,
                unsigned char *kind_out,
                unsigned char **payload_out,
                size_t *len_out )
{
    unsigned char header[RPC_FRAME_HEADER_BYTES];
    uint32_t nlen;
    size_t len;
    unsigned char *buf;
    ssize_t rc;

    *payload_out = NULL;
    *len_out     = 0;

    rc = read_exact( fd, header, RPC_FRAME_HEADER_BYTES );
    if ( rc == 0 ) return 1;
    if ( rc <  0 ) return -1;

    memcpy( &nlen, header, 4 );
    len = (size_t) ntohl( nlen );
    *kind_out = header[4];

    if ( len > RPC_FRAME_MAX_PAYLOAD )
        return -1;

    if ( len == 0 )
        return 0;

    buf = (unsigned char *) malloc( len + 1 );
    if ( buf == NULL )
        return -1;

    rc = read_exact( fd, buf, len );
    if ( rc <= 0 )
    {
        free( buf );
        return -1;
    }
    buf[len] = '\0';

    *payload_out = buf;
    *len_out     = len;
    return 0;
}

/* --- Emitter glue: install for server_core_emit_raw() ------------- */

static void
rpc_line_emitter( const char *buf, size_t len, void *ctx )
{
    RpcContext *c = (RpcContext *) ctx;
    if ( c == NULL || c->fd < 0 )
        return;
    (void) rpc_write_frame( c->fd, RPC_FRAME_KIND_JSON, buf, len );
}

/* --- Rendezvous + token ------------------------------------------- */

static int
rpc_gen_random_bytes( unsigned char *out, size_t n )
{
    int fd = open( "/dev/urandom", O_RDONLY );
    ssize_t rc;
    if ( fd < 0 )
        return -1;
    rc = read( fd, out, n );
    close( fd );
    return ( rc == (ssize_t) n ) ? 0 : -1;
}

static int
rpc_gen_session_id( char *out, size_t cap )
{
    unsigned char raw[4];
    if ( cap < 14 ) return -1;
    if ( rpc_gen_random_bytes( raw, sizeof( raw ) ) != 0 )
        return -1;
    snprintf( out, cap, "griz-%02x%02x%02x%02x",
              raw[0], raw[1], raw[2], raw[3] );
    return 0;
}

/* Create parent directories for `path` with mode 0700 (ignoring EEXIST). */
static int
rpc_mkdir_p( const char *path, mode_t mode )
{
    char tmp[PATH_MAX];
    size_t n;
    char *p;

    n = strlen( path );
    if ( n == 0 || n >= sizeof( tmp ) )
        return -1;
    memcpy( tmp, path, n + 1 );

    for ( p = tmp + 1; *p; p++ )
    {
        if ( *p == '/' )
        {
            *p = '\0';
            if ( mkdir( tmp, mode ) != 0 && errno != EEXIST )
                return -1;
            *p = '/';
        }
    }
    if ( mkdir( tmp, mode ) != 0 && errno != EEXIST )
        return -1;
    return 0;
}

static int
rpc_default_rendezvous_path( const char *session_id, char *out, size_t cap )
{
    const char *home = getenv( "HOME" );
    if ( home == NULL || home[0] == '\0' )
        return -1;
    if ( snprintf( out, cap, "%s/.griz/rendezvous", home )
         >= (int) cap )
        return -1;
    if ( rpc_mkdir_p( out, 0700 ) != 0 )
        return -1;
    if ( snprintf( out, cap, "%s/.griz/rendezvous/%s.json",
                   home, session_id ) >= (int) cap )
        return -1;
    return 0;
}

/* Write {version, session_id, host, port, token, server_pid, started_at}
 * to `path` atomically with mode 0600. */
static int
rpc_write_rendezvous( const char *path,
                      const char *session_id,
                      const char *host,
                      int         port,
                      const char *token_b64 )
{
    cJSON *root;
    char  *txt = NULL;
    char   tmp_path[PATH_MAX];
    int    fd;
    int    rc = -1;

    if ( snprintf( tmp_path, sizeof( tmp_path ), "%s.tmp", path )
         >= (int) sizeof( tmp_path ) )
        return -1;

    root = cJSON_CreateObject();
    if ( root == NULL )
        return -1;
    cJSON_AddNumberToObject( root, "version",     1 );
    cJSON_AddStringToObject( root, "session_id",  session_id );
    cJSON_AddStringToObject( root, "host",        host );
    cJSON_AddNumberToObject( root, "port",        port );
    cJSON_AddStringToObject( root, "token",       token_b64 );
    cJSON_AddNumberToObject( root, "server_pid",  (double) getpid() );
    cJSON_AddNumberToObject( root, "started_at",  (double) time( NULL ) );
    txt = cJSON_PrintUnformatted( root );
    cJSON_Delete( root );
    if ( txt == NULL )
        return -1;

    fd = open( tmp_path, O_WRONLY | O_CREAT | O_TRUNC, 0600 );
    if ( fd < 0 )
    {
        free( txt );
        return -1;
    }
    if ( write_exact( fd, txt, strlen( txt ) ) == 0
         && write_exact( fd, "\n", 1 ) == 0 )
    {
        if ( fsync( fd ) == 0 && rename( tmp_path, path ) == 0 )
            rc = 0;
    }
    close( fd );
    free( txt );
    if ( rc != 0 )
        unlink( tmp_path );
    return rc;
}

/* --- Token validation: constant-time compare ---------------------- */

static int
rpc_const_time_equal( const char *a, const char *b, size_t n )
{
    unsigned char diff = 0;
    size_t i;
    for ( i = 0; i < n; i++ )
        diff |= (unsigned char) ( a[i] ^ b[i] );
    return diff == 0;
}

/* Validate the first received frame as `{"type":"hello", "token":"..."}`.
 * Emits either `hello_ack` (on success) or a `protocol_mismatch` error
 * response (on failure) via the framed emitter. Returns 0 on success,
 * -1 on auth failure. */
static int
rpc_handshake( RpcContext *c,
               const unsigned char *payload, size_t len )
{
    cJSON      *root;
    cJSON      *type_item;
    cJSON      *token_item;
    const char *token_recv;
    int         ok;

    if ( payload == NULL || len == 0 )
    {
        server_emit_error( NULL, "protocol_mismatch",
                           "empty hello frame" );
        return -1;
    }

    root = cJSON_ParseWithLength( (const char *) payload, len );
    if ( root == NULL )
    {
        server_emit_error( NULL, "protocol_mismatch",
                           "malformed hello JSON" );
        return -1;
    }

    type_item = cJSON_GetObjectItemCaseSensitive( root, "type" );
    if ( !cJSON_IsString( type_item )
         || strcmp( type_item->valuestring, "hello" ) != 0 )
    {
        server_emit_error( NULL, "protocol_mismatch",
                           "first frame must be hello" );
        cJSON_Delete( root );
        return -1;
    }

    token_item = cJSON_GetObjectItemCaseSensitive( root, "token" );
    token_recv = cJSON_IsString( token_item ) ? token_item->valuestring : NULL;
    if ( token_recv == NULL )
    {
        server_emit_error( NULL, "protocol_mismatch",
                           "hello missing token" );
        cJSON_Delete( root );
        return -1;
    }

    /* Compare full length of expected token (includes trailing '='). A
     * length mismatch is still run through the constant-time loop over
     * the expected length, with a forced-inequality sentinel on short
     * input, to preserve the timing invariant. */
    {
        size_t expected_len = strlen( c->token_b64 );
        size_t recv_len     = strlen( token_recv );
        char   pad[RPC_TOKEN_B64_LEN];

        memset( pad, 0, sizeof( pad ) );
        if ( recv_len < expected_len )
        {
            memcpy( pad, token_recv, recv_len );
            ok = 0;   /* forced mismatch because pad is zero-padded */
            (void) rpc_const_time_equal( c->token_b64, pad, expected_len );
        }
        else
        {
            ok = rpc_const_time_equal( c->token_b64, token_recv,
                                       expected_len )
                 && recv_len == expected_len;
        }
    }
    cJSON_Delete( root );

    if ( !ok )
    {
        server_emit_error( NULL, "protocol_mismatch",
                           "invalid rendezvous token" );
        return -1;
    }

    /* Auth passed — hand the original hello line back through the
     * shared handshake helper, which emits hello_ack with the version-
     * compatibility bit set. payload is NUL-terminated by
     * rpc_read_frame(). */
    (void) server_try_hello( (const char *) payload );
    return 0;
}

/* --- Signal handling ---------------------------------------------- */

static void
rpc_signal_handler( int signo )
{
    (void) signo;
    g_signal_term_pending = 1;
    /* Best-effort: shut down the client read side so the main loop
     * observes EOF promptly. Avoid close() in a handler — just
     * shutdown() which is async-signal-safe on Linux. */
    if ( g_rpc.fd >= 0 )
        shutdown( g_rpc.fd, SHUT_RD );
}

static void
rpc_install_signals( void )
{
    struct sigaction sa;
    memset( &sa, 0, sizeof( sa ) );
    sa.sa_handler = rpc_signal_handler;
    sigemptyset( &sa.sa_mask );
    sa.sa_flags = 0;
    sigaction( SIGTERM, &sa, NULL );
    sigaction( SIGINT,  &sa, NULL );

    /* SIGPIPE: prefer EPIPE from write() over a process kill. */
    signal( SIGPIPE, SIG_IGN );
}

/* Emit a session_ending event through the current emitter. */
static void
rpc_emit_session_ending( const char *reason, int seconds_remaining )
{
    cJSON *root = cJSON_CreateObject();
    char  *txt;
    if ( root == NULL ) return;
    cJSON_AddStringToObject( root, "type",              "event" );
    cJSON_AddStringToObject( root, "event",             "session_ending" );
    cJSON_AddStringToObject( root, "reason",            reason );
    if ( seconds_remaining >= 0 )
        cJSON_AddNumberToObject( root, "seconds_remaining",
                                 (double) seconds_remaining );
    txt = cJSON_PrintUnformatted( root );
    if ( txt != NULL )
    {
        server_emit_raw( txt );
        free( txt );
    }
    cJSON_Delete( root );
}

/* --- Main entry point --------------------------------------------- */

int
process_server_mode_rpc( const char *db_path,
                         int width, int height,
                         const char *bind_host,
                         int bind_port,
                         const char *rendezvous_path )
{
    int  listen_fd = -1;
    int  client_fd = -1;
    int  opt       = 1;
    struct sockaddr_in addr;
    socklen_t           addr_len = sizeof( addr );
    unsigned char       token_raw[RPC_TOKEN_BYTES];
    char                token_b64[RPC_TOKEN_B64_LEN];
    char                session_id[16];
    char                default_rv[PATH_MAX];
    const char         *rv_path;
    int                 assigned_port;
    Analysis           *analy = NULL;
    int                 rc;
    int                 clean_exit = 0;
    int                 return_code = 0;

    if ( bind_host == NULL || bind_host[0] == '\0' )
        bind_host = "127.0.0.1";

    /* --- Session id + token. --- */
    if ( rpc_gen_session_id( session_id, sizeof( session_id ) ) != 0 )
    {
        fprintf( stderr, "griz-server: failed to generate session id\n" );
        return 1;
    }
    if ( rpc_gen_random_bytes( token_raw, sizeof( token_raw ) ) != 0 )
    {
        fprintf( stderr, "griz-server: failed to read /dev/urandom\n" );
        return 1;
    }
    rpc_base64_encode( token_raw, sizeof( token_raw ), token_b64 );

    /* --- Rendezvous path. --- */
    if ( rendezvous_path != NULL && rendezvous_path[0] != '\0' )
    {
        rv_path = rendezvous_path;
    }
    else if ( rpc_default_rendezvous_path( session_id, default_rv,
                                           sizeof( default_rv ) ) == 0 )
    {
        rv_path = default_rv;
    }
    else
    {
        fprintf( stderr,
                 "griz-server: cannot compute default rendezvous path"
                 " (HOME unset?)\n" );
        return 1;
    }

    /* --- Listen socket. --- */
    listen_fd = socket( AF_INET, SOCK_STREAM, 0 );
    if ( listen_fd < 0 )
    {
        fprintf( stderr, "griz-server: socket() failed: %s\n",
                 strerror( errno ) );
        return 1;
    }
    setsockopt( listen_fd, SOL_SOCKET, SO_REUSEADDR,
                &opt, sizeof( opt ) );

    memset( &addr, 0, sizeof( addr ) );
    addr.sin_family = AF_INET;
    addr.sin_port   = htons( (uint16_t) bind_port );
    if ( inet_pton( AF_INET, bind_host, &addr.sin_addr ) != 1 )
    {
        fprintf( stderr, "griz-server: invalid bind host '%s'\n",
                 bind_host );
        close( listen_fd );
        return 1;
    }
    if ( bind( listen_fd, (struct sockaddr *) &addr, sizeof( addr ) ) != 0 )
    {
        fprintf( stderr, "griz-server: bind failed: %s\n",
                 strerror( errno ) );
        close( listen_fd );
        return 1;
    }
    if ( getsockname( listen_fd, (struct sockaddr *) &addr, &addr_len ) != 0 )
    {
        fprintf( stderr, "griz-server: getsockname failed: %s\n",
                 strerror( errno ) );
        close( listen_fd );
        return 1;
    }
    assigned_port = (int) ntohs( addr.sin_port );
    if ( listen( listen_fd, 1 ) != 0 )
    {
        fprintf( stderr, "griz-server: listen failed: %s\n",
                 strerror( errno ) );
        close( listen_fd );
        return 1;
    }

    /* --- Rendezvous file. --- */
    if ( rpc_write_rendezvous( rv_path, session_id, bind_host,
                               assigned_port, token_b64 ) != 0 )
    {
        fprintf( stderr,
                 "griz-server: failed to write rendezvous file %s: %s\n",
                 rv_path, strerror( errno ) );
        close( listen_fd );
        return 1;
    }
    fprintf( stderr,
             "griz-server: listening on %s:%d, rendezvous=%s\n",
             bind_host, assigned_port, rv_path );

    rpc_install_signals();

    /* --- Accept (single connection). --- */
    {
        struct sockaddr_in peer;
        socklen_t          peer_len = sizeof( peer );
        client_fd = accept( listen_fd,
                            (struct sockaddr *) &peer, &peer_len );
    }
    /* Listen socket no longer needed; v1 is single-connection. */
    close( listen_fd );
    listen_fd = -1;

    if ( client_fd < 0 )
    {
        fprintf( stderr, "griz-server: accept failed: %s\n",
                 strerror( errno ) );
        unlink( rv_path );
        return 1;
    }

    /* TCP_NODELAY: responses are latency-sensitive, frames are big —
     * neither benefits from Nagle. */
    setsockopt( client_fd, IPPROTO_TCP, TCP_NODELAY,
                &opt, sizeof( opt ) );

    g_rpc.fd = client_fd;
    memcpy( g_rpc.token_b64, token_b64, sizeof( token_b64 ) );

    /* Install the framed emitter for all subsequent JSON output. */
    server_set_line_emitter( rpc_line_emitter, &g_rpc );

    /* --- Handshake (token auth). --- */
    {
        unsigned char *payload = NULL;
        size_t         len     = 0;
        unsigned char  kind    = 0;
        int            r;

        r = rpc_read_frame( client_fd, &kind, &payload, &len );
        if ( r != 0 || kind != RPC_FRAME_KIND_JSON )
        {
            server_emit_error( NULL, "protocol_mismatch",
                               "first frame must be JSON hello" );
            free( payload );
            return_code = 1;
            goto cleanup;
        }
        r = rpc_handshake( &g_rpc, payload, len );
        free( payload );
        if ( r != 0 )
        {
            return_code = 1;
            goto cleanup;
        }
    }

    /* --- Analysis + DB + OSMesa startup. --- */
    rc = server_core_startup( &analy, db_path, width, height );
    if ( rc != 0 )
    {
        server_emit_error( NULL, "internal_error",
                           "server startup failed" );
        return_code = rc;
        goto cleanup;
    }
    server_emit_ready();

    /* --- Dispatch loop. --- */
    while ( !g_signal_term_pending )
    {
        unsigned char *payload = NULL;
        size_t         len     = 0;
        unsigned char  kind    = 0;
        int            r;

        r = rpc_read_frame( client_fd, &kind, &payload, &len );
        if ( r == 1 )  /* clean peer close */
        {
            free( payload );
            clean_exit = 1;
            break;
        }
        if ( r != 0 )
        {
            free( payload );
            break;
        }

        if ( kind == RPC_FRAME_KIND_HEARTBEAT )
        {
            /* Echo the payload back so the client can compute RTT. */
            (void) rpc_write_frame( client_fd, RPC_FRAME_KIND_HEARTBEAT,
                                    payload, len );
            free( payload );
            continue;
        }
        if ( kind != RPC_FRAME_KIND_JSON )
        {
            server_emit_error( NULL, "internal_error",
                               "unsupported frame kind" );
            free( payload );
            break;
        }

        if ( server_core_dispatch_line( (const char *) payload, analy ) == 1 )
        {
            free( payload );
            clean_exit = 1;
            break;
        }
        free( payload );
    }

    if ( g_signal_term_pending )
    {
        rpc_emit_session_ending( "signal_term", 0 );
    }
    else if ( !clean_exit )
    {
        rpc_emit_session_ending( "peer_idle", 0 );
    }

    server_core_history_cleanup( analy );

cleanup:
    if ( client_fd >= 0 )
    {
        shutdown( client_fd, SHUT_RDWR );
        close( client_fd );
    }
    g_rpc.fd = -1;
    unlink( rv_path );

    /* Restore default emitter in case anything later in the process
     * still wants to print through stdout. */
    server_set_line_emitter( NULL, NULL );
    return return_code;
}

#endif /* GRIZ_SERVER_BUILD */
