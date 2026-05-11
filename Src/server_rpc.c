/*
 * server_rpc.c - Length-framed RPC transport for griz-server.
 *
 * See server_rpc.h for the entry point contract and planning/ui-design/
 * 02-protocol.md for the wire format. Multi-client support (up to
 * RPC_MAX_CLIENTS concurrent peers) is documented in planning/DEMO.md
 * task D — the server keeps its listen socket open after the first
 * accept so a second client (e.g. griz-mcp in attach mode) can share the
 * same Analysis/OSMesa state.
 *
 * Routing:
 *   - Responses and hello_ack go to the currently-dispatching client
 *     only. The line emitter consults g_rpc.current_client_idx.
 *   - state_changed / session_ending events and binary auto-push frames
 *     broadcast to every authenticated client. Signal-term
 *     session_ending broadcasts; per-client peer_idle is sent only to
 *     the offending client.
 *
 * Post-MVP (tracked in planning/UI.md § Phase 2):
 *   - Three-thread split (command / render / I-O) with bounded queues.
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
#include <poll.h>
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
#include "server_render.h"
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

/* --- Keepalive timing (02-protocol.md §2.4) ----------------------- */
#define RPC_HEARTBEAT_INTERVAL_MS (20 * 1000)  /* send every 20 s of outbound idle */
#define RPC_PEER_IDLE_TIMEOUT_MS  (60 * 1000)  /* tear down after 60 s of inbound silence */
#define RPC_POLL_TICK_MS          (1 * 1000)   /* wake up at least once a second so the
                                                  signal handler and timers get serviced */

/* --- Multi-client cap. -------------------------------------------- */
/* DEMO.md task D: 2 is the real-world case (Qt UI + MCP); 4 is a safety
 * cap so a misbehaving peer can't exhaust slots. */
#define RPC_MAX_CLIENTS           4

/* --- Connection state --------------------------------------------- */

typedef struct {
    int       fd;                             /* -1 means slot empty */
    int       authenticated;                  /* 0 until hello + token validated */
    long long last_inbound_ms;
    long long last_outbound_ms;
} RpcClient;

typedef struct {
    int        listen_fd;
    char       token_b64[RPC_TOKEN_B64_LEN];
    RpcClient  clients[RPC_MAX_CLIENTS];
    /* Index of the client currently being dispatched; response-style
     * emits target this client only. -1 when no dispatch is active
     * (e.g. during signal-term broadcast). */
    int        current_client_idx;
    int        startup_done;
} RpcServer;

/* Module-level singleton so the signal handler can reach every fd. */
static RpcServer g_rpc;
static volatile sig_atomic_t g_signal_term_pending = 0;

static void
rpc_server_init( RpcServer *s )
{
    int i;
    s->listen_fd = -1;
    s->token_b64[0] = '\0';
    s->current_client_idx = -1;
    s->startup_done = 0;
    for ( i = 0; i < RPC_MAX_CLIENTS; i++ )
    {
        s->clients[i].fd               = -1;
        s->clients[i].authenticated    = 0;
        s->clients[i].last_inbound_ms  = 0;
        s->clients[i].last_outbound_ms = 0;
    }
}

/* Monotonic-clock milliseconds. Used for heartbeat and peer-idle
 * bookkeeping; CLOCK_MONOTONIC is immune to wall-clock jumps. */
static long long
rpc_now_ms( void )
{
    struct timespec ts;
    if ( clock_gettime( CLOCK_MONOTONIC, &ts ) != 0 )
        return 0;
    return (long long) ts.tv_sec * 1000LL
         + (long long) ts.tv_nsec / 1000000LL;
}

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

/* Write one length-prefixed frame to a specific client slot. On
 * success, refreshes that client's last_outbound_ms so the keepalive
 * loop doesn't double-send a heartbeat on top of an otherwise busy
 * link. Returns 0 on success, -1 on short write (caller typically
 * closes that slot). */
static int
rpc_write_frame_to( RpcClient *c, unsigned char kind,
                    const void *payload, size_t len )
{
    unsigned char header[RPC_FRAME_HEADER_BYTES];
    uint32_t nlen;

    if ( c == NULL || c->fd < 0 )
        return -1;
    if ( len > RPC_FRAME_MAX_PAYLOAD )
        return -1;

    nlen = htonl( (uint32_t) len );
    memcpy( header, &nlen, 4 );
    header[4] = kind;

    if ( write_exact( c->fd, header, RPC_FRAME_HEADER_BYTES ) != 0 )
        return -1;
    if ( len > 0 && write_exact( c->fd, payload, len ) != 0 )
        return -1;

    c->last_outbound_ms = rpc_now_ms();
    return 0;
}

/* Close a client slot and reset its bookkeeping. Safe to call on an
 * already-closed slot. */
static void
rpc_close_client( RpcClient *c )
{
    if ( c == NULL ) return;
    if ( c->fd >= 0 )
    {
        shutdown( c->fd, SHUT_RDWR );
        close( c->fd );
        c->fd = -1;
    }
    c->authenticated    = 0;
    c->last_inbound_ms  = 0;
    c->last_outbound_ms = 0;
}

/* Wait up to `timeout_ms` for fd to become readable. Returns 1 if
 * readable, 0 on timeout, -1 on error. EINTR is surfaced as 0 so the
 * main loop's signal check can run. */
static int
rpc_poll_readable( int fd, int timeout_ms )
{
    struct pollfd pfd;
    int rc;

    pfd.fd      = fd;
    pfd.events  = POLLIN;
    pfd.revents = 0;

    rc = poll( &pfd, 1, timeout_ms );
    if ( rc < 0 )
        return ( errno == EINTR ) ? 0 : -1;
    if ( rc == 0 )
        return 0;
    if ( pfd.revents & ( POLLIN | POLLHUP | POLLERR ) )
        return 1;
    return 0;
}

/* Read one frame from a specific fd. Allocates `*payload_out` (freed
 * by caller) and populates `*kind_out`, `*len_out`. Returns:
 *    0 = frame read ok
 *    1 = peer closed cleanly
 *   -1 = error (oversize, short read, protocol breakage)
 */
static int
rpc_read_frame_fd( int fd,
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

/* --- Emitter glue: install for server_core_emit_raw() -------------
 *
 * The line emitter takes an `is_response` hint so we can route
 * response-shaped frames to the currently-dispatching client only
 * while broadcasting events to everyone. Binary frames are always
 * broadcast for now — the Qt UI should paint any rendered frame even
 * if it was triggered by another client (exactly what the DEMO wants).
 */

static void
rpc_line_emitter( const char *buf, size_t len,
                  int is_response, void *ctx )
{
    RpcServer *s = (RpcServer *) ctx;
    int i;

    if ( s == NULL || buf == NULL || len == 0 )
        return;

    if ( is_response )
    {
        if ( s->current_client_idx < 0
             || s->current_client_idx >= RPC_MAX_CLIENTS )
            return;
        RpcClient *c = &s->clients[s->current_client_idx];
        if ( c->fd < 0 )
            return;
        if ( rpc_write_frame_to( c, RPC_FRAME_KIND_JSON, buf, len ) != 0 )
            rpc_close_client( c );
        return;
    }

    /* Broadcast: fan out to every authenticated client. Unauthenticated
     * slots are skipped — they might still be in mid-handshake and
     * should not see someone else's state_changed event. */
    for ( i = 0; i < RPC_MAX_CLIENTS; i++ )
    {
        RpcClient *c = &s->clients[i];
        if ( c->fd < 0 || !c->authenticated )
            continue;
        if ( rpc_write_frame_to( c, RPC_FRAME_KIND_JSON, buf, len ) != 0 )
            rpc_close_client( c );
    }
}

/* Binary-frame emitter. Broadcast to every authenticated client so the
 * Qt UI paints frames triggered by the MCP peer (and vice versa). */
static void
rpc_binary_emitter( const unsigned char *payload, size_t len, void *ctx )
{
    RpcServer *s = (RpcServer *) ctx;
    int i;

    if ( s == NULL || payload == NULL || len == 0 )
        return;

    for ( i = 0; i < RPC_MAX_CLIENTS; i++ )
    {
        RpcClient *c = &s->clients[i];
        if ( c->fd < 0 || !c->authenticated )
            continue;
        if ( rpc_write_frame_to( c, RPC_FRAME_KIND_BINARY, payload, len ) != 0 )
            rpc_close_client( c );
    }
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

/* Validate a hello frame's token. Emits hello_ack via server_try_hello
 * on success; emits protocol_mismatch error on failure. Both go through
 * the current-client-routed emitter since current_client_idx is set to
 * this client before the call. Returns 0 on success, -1 on auth
 * failure. */
static int
rpc_handshake_validate( const char *expected_token_b64,
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
        size_t expected_len = strlen( expected_token_b64 );
        size_t recv_len     = strlen( token_recv );
        char   pad[RPC_TOKEN_B64_LEN];

        memset( pad, 0, sizeof( pad ) );
        if ( recv_len < expected_len )
        {
            memcpy( pad, token_recv, recv_len );
            ok = 0;   /* forced mismatch because pad is zero-padded */
            (void) rpc_const_time_equal( expected_token_b64, pad, expected_len );
        }
        else
        {
            ok = rpc_const_time_equal( expected_token_b64, token_recv,
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
     * rpc_read_frame_fd(). */
    (void) server_try_hello( (const char *) payload );
    return 0;
}

/* --- Signal handling ---------------------------------------------- */

static void
rpc_signal_handler( int signo )
{
    int i;
    (void) signo;
    g_signal_term_pending = 1;
    /* Best-effort: shut down each client's read side so the main loop
     * observes EOF promptly. shutdown() is async-signal-safe on Linux. */
    for ( i = 0; i < RPC_MAX_CLIENTS; i++ )
    {
        if ( g_rpc.clients[i].fd >= 0 )
            shutdown( g_rpc.clients[i].fd, SHUT_RD );
    }
    /* Wake an accept() on listen_fd too so a quiet server with no
     * clients still exits promptly. */
    if ( g_rpc.listen_fd >= 0 )
        shutdown( g_rpc.listen_fd, SHUT_RD );
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

/* Emit a session_ending event. Broadcasts to every authenticated
 * client by going through the broadcast channel (server_emit_raw). */
static void
rpc_emit_session_ending_broadcast( const char *reason, int seconds_remaining )
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

/* Per-client session_ending. Uses a targeted write rather than the
 * broadcast channel so a single peer-idle timeout doesn't nudge the
 * other clients. */
static void
rpc_emit_session_ending_to( RpcClient *c, const char *reason )
{
    cJSON *root = cJSON_CreateObject();
    char  *txt;
    if ( root == NULL || c == NULL || c->fd < 0 )
    {
        if ( root != NULL ) cJSON_Delete( root );
        return;
    }
    cJSON_AddStringToObject( root, "type",   "event" );
    cJSON_AddStringToObject( root, "event",  "session_ending" );
    cJSON_AddStringToObject( root, "reason", reason );
    cJSON_AddNumberToObject( root, "seconds_remaining", 0 );
    txt = cJSON_PrintUnformatted( root );
    if ( txt != NULL )
    {
        (void) rpc_write_frame_to( c, RPC_FRAME_KIND_JSON,
                                   txt, strlen( txt ) );
        free( txt );
    }
    cJSON_Delete( root );
}

/* Find the next free client slot, or -1 if all slots are taken. */
static int
rpc_find_free_slot( RpcServer *s )
{
    int i;
    for ( i = 0; i < RPC_MAX_CLIENTS; i++ )
        if ( s->clients[i].fd < 0 )
            return i;
    return -1;
}

/* Count authenticated client slots. Used only for bookkeeping/logging. */
static int
rpc_authenticated_count( RpcServer *s )
{
    int i, n = 0;
    for ( i = 0; i < RPC_MAX_CLIENTS; i++ )
        if ( s->clients[i].fd >= 0 && s->clients[i].authenticated )
            n++;
    return n;
}

/* Accept a new connection off the listen socket and install it in a
 * free slot. Returns the slot index on success, -1 if no slot is free
 * (in which case the connection is closed immediately) or accept
 * failed. */
static int
rpc_accept_new_client( RpcServer *s )
{
    int fd;
    int opt = 1;
    int slot;
    struct sockaddr_in peer;
    socklen_t peer_len = sizeof( peer );

    fd = accept( s->listen_fd, (struct sockaddr *) &peer, &peer_len );
    if ( fd < 0 )
    {
        if ( errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK )
            fprintf( stderr, "griz-server: accept failed: %s\n",
                     strerror( errno ) );
        return -1;
    }

    slot = rpc_find_free_slot( s );
    if ( slot < 0 )
    {
        /* All slots taken — reject politely and keep serving the rest. */
        fprintf( stderr,
                 "griz-server: refusing new client: all %d slots in use\n",
                 RPC_MAX_CLIENTS );
        shutdown( fd, SHUT_RDWR );
        close( fd );
        return -1;
    }

    setsockopt( fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof( opt ) );

    s->clients[slot].fd               = fd;
    s->clients[slot].authenticated    = 0;
    s->clients[slot].last_inbound_ms  = rpc_now_ms();
    s->clients[slot].last_outbound_ms = s->clients[slot].last_inbound_ms;
    return slot;
}

/* --- Main entry point --------------------------------------------- *
 *
 * The server_core_startup() call is scheduled to run on the FIRST
 * client handshake (see below). server_core_dispatch_line() is called
 * with current_client_idx set so response-shaped emits route correctly.
 */

int
process_server_mode_rpc( const char *db_path,
                         int width, int height,
                         const char *bind_host,
                         int bind_port,
                         const char *rendezvous_path )
{
    int                 opt       = 1;
    struct sockaddr_in  addr;
    socklen_t           addr_len = sizeof( addr );
    unsigned char       token_raw[RPC_TOKEN_BYTES];
    char                session_id[16];
    char                default_rv[PATH_MAX];
    const char         *rv_path;
    int                 assigned_port;
    Analysis           *analy = NULL;
    int                 return_code = 0;
    int                 startup_rc;

    rpc_server_init( &g_rpc );

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
    rpc_base64_encode( token_raw, sizeof( token_raw ), g_rpc.token_b64 );

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
    g_rpc.listen_fd = socket( AF_INET, SOCK_STREAM, 0 );
    if ( g_rpc.listen_fd < 0 )
    {
        fprintf( stderr, "griz-server: socket() failed: %s\n",
                 strerror( errno ) );
        return 1;
    }
    setsockopt( g_rpc.listen_fd, SOL_SOCKET, SO_REUSEADDR,
                &opt, sizeof( opt ) );

    memset( &addr, 0, sizeof( addr ) );
    addr.sin_family = AF_INET;
    addr.sin_port   = htons( (uint16_t) bind_port );
    if ( inet_pton( AF_INET, bind_host, &addr.sin_addr ) != 1 )
    {
        fprintf( stderr, "griz-server: invalid bind host '%s'\n", bind_host );
        close( g_rpc.listen_fd );
        g_rpc.listen_fd = -1;
        return 1;
    }
    if ( bind( g_rpc.listen_fd, (struct sockaddr *) &addr, sizeof( addr ) ) != 0 )
    {
        fprintf( stderr, "griz-server: bind failed: %s\n", strerror( errno ) );
        close( g_rpc.listen_fd );
        g_rpc.listen_fd = -1;
        return 1;
    }
    if ( getsockname( g_rpc.listen_fd, (struct sockaddr *) &addr, &addr_len ) != 0 )
    {
        fprintf( stderr, "griz-server: getsockname failed: %s\n",
                 strerror( errno ) );
        close( g_rpc.listen_fd );
        g_rpc.listen_fd = -1;
        return 1;
    }
    assigned_port = (int) ntohs( addr.sin_port );
    if ( listen( g_rpc.listen_fd, RPC_MAX_CLIENTS ) != 0 )
    {
        fprintf( stderr, "griz-server: listen failed: %s\n",
                 strerror( errno ) );
        close( g_rpc.listen_fd );
        g_rpc.listen_fd = -1;
        return 1;
    }

    /* --- Rendezvous file. --- */
    if ( rpc_write_rendezvous( rv_path, session_id, bind_host,
                               assigned_port, g_rpc.token_b64 ) != 0 )
    {
        fprintf( stderr,
                 "griz-server: failed to write rendezvous file %s: %s\n",
                 rv_path, strerror( errno ) );
        close( g_rpc.listen_fd );
        g_rpc.listen_fd = -1;
        return 1;
    }
    fprintf( stderr,
             "griz-server: listening on %s:%d, rendezvous=%s (max %d clients)\n",
             bind_host, assigned_port, rv_path, RPC_MAX_CLIENTS );

    rpc_install_signals();

    /* Install the framed emitters for all subsequent output. Must be
     * in place before we accept the first client so their hello_ack
     * goes through the RPC channel. */
    server_set_line_emitter(   rpc_line_emitter,   &g_rpc );
    server_set_binary_emitter( rpc_binary_emitter, &g_rpc );

    /* --- Wait for the first client, handshake, then run startup. ---
     * Startup runs exactly once per server lifetime, triggered by the
     * first successful handshake. Subsequent clients share the Analy
     * and OSMesa context. */
    {
        int first_slot = -1;
        int pr;

        while ( !g_signal_term_pending && first_slot < 0 )
        {
            pr = rpc_poll_readable( g_rpc.listen_fd, RPC_POLL_TICK_MS );
            if ( pr < 0 )
            {
                return_code = 1;
                goto cleanup;
            }
            if ( pr == 0 )
                continue;

            first_slot = rpc_accept_new_client( &g_rpc );
        }
        if ( g_signal_term_pending || first_slot < 0 )
        {
            return_code = 1;
            goto cleanup;
        }

        /* Read first frame from the first client, validate hello + token. */
        {
            unsigned char *payload = NULL;
            size_t         len     = 0;
            unsigned char  kind    = 0;
            int            pr_client;
            int            r;

            pr_client = rpc_poll_readable( g_rpc.clients[first_slot].fd,
                                           RPC_PEER_IDLE_TIMEOUT_MS );
            if ( pr_client <= 0 )
            {
                g_rpc.current_client_idx = first_slot;
                if ( pr_client == 0 )
                    server_emit_error( NULL, "protocol_mismatch",
                                       "hello not received within idle timeout" );
                g_rpc.current_client_idx = -1;
                rpc_close_client( &g_rpc.clients[first_slot] );
                return_code = 1;
                goto cleanup;
            }

            r = rpc_read_frame_fd( g_rpc.clients[first_slot].fd,
                                   &kind, &payload, &len );
            if ( r != 0 || kind != RPC_FRAME_KIND_JSON )
            {
                g_rpc.current_client_idx = first_slot;
                server_emit_error( NULL, "protocol_mismatch",
                                   "first frame must be JSON hello" );
                g_rpc.current_client_idx = -1;
                free( payload );
                rpc_close_client( &g_rpc.clients[first_slot] );
                return_code = 1;
                goto cleanup;
            }
            g_rpc.clients[first_slot].last_inbound_ms = rpc_now_ms();

            g_rpc.current_client_idx = first_slot;
            r = rpc_handshake_validate( g_rpc.token_b64, payload, len );
            g_rpc.current_client_idx = -1;
            free( payload );
            if ( r != 0 )
            {
                rpc_close_client( &g_rpc.clients[first_slot] );
                return_code = 1;
                goto cleanup;
            }
            g_rpc.clients[first_slot].authenticated = 1;
        }

        /* --- Analysis + DB + OSMesa startup. --- */
        startup_rc = server_core_startup( &analy, db_path, width, height );
        if ( startup_rc != 0 )
        {
            g_rpc.current_client_idx = first_slot;
            server_emit_error( NULL, "internal_error",
                               "server startup failed" );
            g_rpc.current_client_idx = -1;
            return_code = startup_rc;
            goto cleanup;
        }
        g_rpc.startup_done = 1;

        g_rpc.current_client_idx = first_slot;
        server_emit_ready();
        g_rpc.current_client_idx = -1;
    }

    /* --- Dispatch loop. ---
     * On each iteration:
     *   1. Per-client heartbeat: emit kind=0x03 to any client that's
     *      been outbound-idle for RPC_HEARTBEAT_INTERVAL_MS.
     *   2. Build the poll set (listen_fd + every connected client fd).
     *   3. poll() with a timeout scaled to the nearest upcoming timer
     *      event across all clients (or RPC_POLL_TICK_MS, whichever is
     *      smaller) so signals/frame-flush get a prompt chance.
     *   4. On accept-readable: accept; handshake synchronously.
     *   5. On client-readable: read one frame.
     *        - Heartbeat: echo.
     *        - JSON: set current_client_idx; dispatch; clear.
     *   6. On pure timeout: flush any deferred render frame, then check
     *      peer_idle per-client and close the ones that crossed the line.
     */
    while ( !g_signal_term_pending )
    {
        struct pollfd pfds[RPC_MAX_CLIENTS + 1];
        int           pfd_slot[RPC_MAX_CLIENTS + 1]; /* back-map to clients[] */
        int           pfd_n = 0;
        long long     now_ms;
        int           timeout_ms;
        int           i;
        int           pr;

        now_ms = rpc_now_ms();

        /* Per-client heartbeat emission. */
        for ( i = 0; i < RPC_MAX_CLIENTS; i++ )
        {
            RpcClient *c = &g_rpc.clients[i];
            if ( c->fd < 0 || !c->authenticated )
                continue;
            if ( now_ms - c->last_outbound_ms >= RPC_HEARTBEAT_INTERVAL_MS )
            {
                (void) rpc_write_frame_to( c, RPC_FRAME_KIND_HEARTBEAT,
                                           NULL, 0 );
            }
        }

        /* Compute shortest timeout across listen_fd + every client. */
        timeout_ms = RPC_POLL_TICK_MS;
        for ( i = 0; i < RPC_MAX_CLIENTS; i++ )
        {
            RpcClient *c = &g_rpc.clients[i];
            long long  outbound_remaining;
            long long  inbound_remaining;
            int        per_client_ms;

            if ( c->fd < 0 )
                continue;

            outbound_remaining = RPC_HEARTBEAT_INTERVAL_MS
                                 - ( now_ms - c->last_outbound_ms );
            inbound_remaining  = RPC_PEER_IDLE_TIMEOUT_MS
                                 - ( now_ms - c->last_inbound_ms );
            if ( outbound_remaining < 0 ) outbound_remaining = 0;
            if ( inbound_remaining  < 0 ) inbound_remaining  = 0;

            per_client_ms = (int) ( ( outbound_remaining < inbound_remaining )
                                    ? outbound_remaining : inbound_remaining );
            if ( per_client_ms < timeout_ms )
                timeout_ms = per_client_ms;
        }
        /* Deferred frame flush (30 Hz rate limiter, 05 §5.3). */
        {
            int frame_wait_ms = server_render_ms_until_next_frame();
            if ( frame_wait_ms >= 0 && frame_wait_ms < timeout_ms )
                timeout_ms = frame_wait_ms;
        }
        if ( timeout_ms < 0 ) timeout_ms = 0;

        /* Build pollfd set. */
        pfds[pfd_n].fd      = g_rpc.listen_fd;
        pfds[pfd_n].events  = POLLIN;
        pfds[pfd_n].revents = 0;
        pfd_slot[pfd_n]     = -1;   /* listen socket sentinel */
        pfd_n++;

        for ( i = 0; i < RPC_MAX_CLIENTS; i++ )
        {
            if ( g_rpc.clients[i].fd < 0 )
                continue;
            pfds[pfd_n].fd      = g_rpc.clients[i].fd;
            pfds[pfd_n].events  = POLLIN;
            pfds[pfd_n].revents = 0;
            pfd_slot[pfd_n]     = i;
            pfd_n++;
        }

        pr = poll( pfds, pfd_n, timeout_ms );
        if ( pr < 0 )
        {
            if ( errno == EINTR )
                continue;
            break;
        }
        if ( pr == 0 )
        {
            /* Pure timeout: flush any deferred frame, then check
             * per-client peer_idle. */
            (void) server_render_flush_deferred_if_due( analy );

            now_ms = rpc_now_ms();
            for ( i = 0; i < RPC_MAX_CLIENTS; i++ )
            {
                RpcClient *c = &g_rpc.clients[i];
                if ( c->fd < 0 || !c->authenticated )
                    continue;
                if ( now_ms - c->last_inbound_ms >= RPC_PEER_IDLE_TIMEOUT_MS )
                {
                    rpc_emit_session_ending_to( c, "peer_idle" );
                    rpc_close_client( c );
                }
            }
            continue;
        }

        /* Handle new accepts first so a burst of simultaneous connects
         * doesn't starve on existing traffic. */
        if ( pfds[0].revents & ( POLLIN | POLLHUP | POLLERR ) )
        {
            int new_slot = rpc_accept_new_client( &g_rpc );
            if ( new_slot >= 0 )
            {
                /* We don't do a synchronous hello-wait here so other
                 * clients keep making progress. Instead, wait for the
                 * first frame from the new slot on the next iteration
                 * of the poll loop. The peer-idle window applies to
                 * the handshake too, matching 02-protocol.md §7. */
            }
        }

        /* Per-client data. */
        for ( i = 1; i < pfd_n; i++ )
        {
            int slot = pfd_slot[i];
            RpcClient *c;
            unsigned char *payload = NULL;
            size_t         len     = 0;
            unsigned char  kind    = 0;
            int            r;

            if ( !( pfds[i].revents & ( POLLIN | POLLHUP | POLLERR ) ) )
                continue;
            if ( slot < 0 || slot >= RPC_MAX_CLIENTS )
                continue;
            c = &g_rpc.clients[slot];
            if ( c->fd < 0 )
                continue;

            r = rpc_read_frame_fd( c->fd, &kind, &payload, &len );
            if ( r == 1 )  /* clean peer close */
            {
                free( payload );
                rpc_close_client( c );
                continue;
            }
            if ( r != 0 )  /* transport error */
            {
                free( payload );
                rpc_close_client( c );
                continue;
            }

            c->last_inbound_ms = rpc_now_ms();

            if ( kind == RPC_FRAME_KIND_HEARTBEAT )
            {
                (void) rpc_write_frame_to( c, RPC_FRAME_KIND_HEARTBEAT,
                                           payload, len );
                free( payload );
                continue;
            }
            if ( kind != RPC_FRAME_KIND_JSON )
            {
                g_rpc.current_client_idx = slot;
                server_emit_error( NULL, "internal_error",
                                   "unsupported frame kind" );
                g_rpc.current_client_idx = -1;
                free( payload );
                rpc_close_client( c );
                continue;
            }

            /* Unauthenticated slot: the JSON frame must be a hello. */
            if ( !c->authenticated )
            {
                g_rpc.current_client_idx = slot;
                r = rpc_handshake_validate( g_rpc.token_b64, payload, len );
                if ( r != 0 )
                {
                    g_rpc.current_client_idx = -1;
                    free( payload );
                    rpc_close_client( c );
                    continue;
                }
                c->authenticated = 1;
                server_emit_ready();
                g_rpc.current_client_idx = -1;
                free( payload );
                fprintf( stderr,
                         "griz-server: client attached in slot %d (total=%d)\n",
                         slot, rpc_authenticated_count( &g_rpc ) );
                continue;
            }

            /* Authenticated: dispatch. */
            g_rpc.current_client_idx = slot;
            r = server_core_dispatch_line( (const char *) payload, analy );
            g_rpc.current_client_idx = -1;
            free( payload );
            if ( r == 1 )
            {
                /* `quit` / `exit` / `end` terminates the server for
                 * every client (DEMO.md D.9 — the Qt UI's quit tears
                 * the whole process down). */
                goto drain_and_exit;
            }
        }
    }

drain_and_exit:

    if ( g_signal_term_pending )
    {
        rpc_emit_session_ending_broadcast( "signal_term", 0 );
    }
    else
    {
        /* Clean exit driven by a quit command: let peers know. */
        rpc_emit_session_ending_broadcast( "server_quit", 0 );
    }

    server_core_history_cleanup( analy );

cleanup:
    {
        int i;
        for ( i = 0; i < RPC_MAX_CLIENTS; i++ )
            rpc_close_client( &g_rpc.clients[i] );
    }
    if ( g_rpc.listen_fd >= 0 )
    {
        close( g_rpc.listen_fd );
        g_rpc.listen_fd = -1;
    }
    unlink( rv_path );

    /* Restore default emitters in case anything later in the process
     * still wants to print through stdout. */
    server_set_line_emitter(   NULL, NULL );
    server_set_binary_emitter( NULL, NULL );
    return return_code;
}

#endif /* GRIZ_SERVER_BUILD */
