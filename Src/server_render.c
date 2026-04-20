/*
 * server_render.c - see server_render.h for the contract.
 */

#ifdef GRIZ_SERVER_BUILD

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <time.h>

#include <GL/gl.h>
#include <GL/osmesa.h>

#include <png.h>
#include "jpeglib.h"
#include "jerror.h"

#include "cJSON.h"

#include "viewer.h"
#include "draw.h"
#include "server_core.h"
#include "server_render.h"

/* Module-level frame_seq counter. Protected implicitly by the single-
 * threaded v0 dispatch loop; when the three-thread split lands
 * (planning/UI.md Phase 2), this counter moves to the render thread. */
static unsigned long long s_frame_seq = 0;

/* Buffer we've allocated here for the OSMesa framebuffer. The initial
 * buffer passed to OSMesaMakeCurrent() by OffscreenContext() is not
 * tracked anywhere (legacy), so it leaks on the first resize. Callers
 * of server_render_resize_viewport() after that get the buffer freed
 * cleanly. */
static unsigned char *s_osmesa_buffer = NULL;

/* Per-axis cap from 05-rendering-and-streaming.md §2.1. */
#define SERVER_RENDER_VIEWPORT_MAX 4096

/* Pulled in from viewer.c. */
extern OSMesaContext OSMesa_ctx;

unsigned long long
server_render_next_frame_seq( void )
{
    return ++s_frame_seq;
}

int
server_render_capture_rgba( Analysis *analy,
                            unsigned char **out_rgba,
                            int *out_w,
                            int *out_h )
{
    int            w;
    int            h;
    unsigned char *buf;

    if ( analy == NULL || out_rgba == NULL || out_w == NULL || out_h == NULL )
        return -1;

    *out_rgba = NULL;
    *out_w    = 0;
    *out_h    = 0;

    /* Flush any dirty-view state first. update_display() is the single
     * render entry used by every command handler that dirties the scene
     * (05-rendering-and-streaming.md §3). Under OSMesa this runs fully
     * synchronously; glFinish is belt-and-suspenders. */
    if ( analy->update_display != NULL )
        analy->update_display( analy );

    glFinish();

    if ( v_win == NULL )
        return -1;

    w = v_win->vp_width;
    h = v_win->vp_height;
    if ( w <= 0 || h <= 0 )
        return -1;

    buf = (unsigned char *) malloc( (size_t) w * (size_t) h * 4u );
    if ( buf == NULL )
        return -1;

    /* screen_to_memory() handles glReadBuffer() selection for us and
     * calls glReadPixels for RGBA8. OpenGL writes bottom-up; we flip
     * rows to the top-down layout that PNG / JPEG expect so downstream
     * encoders can consume the buffer as-is. */
    screen_to_memory( TRUE, w, h, buf );

    {
        size_t         row_bytes = (size_t) w * 4u;
        unsigned char *tmp = (unsigned char *) malloc( row_bytes );
        int            i;
        if ( tmp == NULL )
        {
            free( buf );
            return -1;
        }
        for ( i = 0; i < h / 2; i++ )
        {
            unsigned char *top = buf + (size_t) i * row_bytes;
            unsigned char *bot = buf + (size_t) ( h - 1 - i ) * row_bytes;
            memcpy( tmp, top,  row_bytes );
            memcpy( top, bot,  row_bytes );
            memcpy( bot, tmp,  row_bytes );
        }
        free( tmp );
    }

    *out_rgba = buf;
    *out_w    = w;
    *out_h    = h;
    return 0;
}

/* ------------------------------------------------------------------
 * PNG: in-memory encoder.
 *
 * libpng doesn't ship a built-in in-memory sink, so we wire a growable
 * byte buffer through png_set_write_fn(). `PngMemSink` is the user_ctx
 * threaded through the libpng callbacks.
 * ------------------------------------------------------------------ */

typedef struct {
    unsigned char *buf;
    size_t         len;
    size_t         cap;
    int            err;   /* sticky: 1 on any allocation failure */
} PngMemSink;

static void
png_mem_write_cb( png_structp png_ptr, png_bytep data, png_size_t len )
{
    PngMemSink *s = (PngMemSink *) png_get_io_ptr( png_ptr );
    if ( s == NULL || s->err )
        return;

    if ( s->len + len > s->cap )
    {
        size_t new_cap = s->cap ? s->cap * 2 : 64 * 1024;
        while ( new_cap < s->len + len )
            new_cap *= 2;
        {
            unsigned char *nb = (unsigned char *) realloc( s->buf, new_cap );
            if ( nb == NULL )
            {
                s->err = 1;
                png_error( png_ptr, "PngMemSink: out of memory" );
                return;
            }
            s->buf = nb;
            s->cap = new_cap;
        }
    }
    memcpy( s->buf + s->len, data, len );
    s->len += len;
}

static void
png_mem_flush_cb( png_structp png_ptr )
{
    (void) png_ptr;
}

int
server_render_encode_png( const unsigned char *rgba,
                          int width, int height, int alpha,
                          unsigned char **out_png,
                          size_t *out_len )
{
    png_structp   png_ptr     = NULL;
    png_infop     info_ptr    = NULL;
    png_bytep    *row_ptrs    = NULL;
    PngMemSink    sink        = { 0 };
    int           channels;
    int           color_type;
    size_t        row_bytes;
    int           i;

    if ( rgba == NULL || out_png == NULL || out_len == NULL
         || width <= 0 || height <= 0 )
        return -1;

    *out_png = NULL;
    *out_len = 0;

    channels   = alpha ? 4 : 3;
    color_type = alpha ? PNG_COLOR_TYPE_RGB_ALPHA : PNG_COLOR_TYPE_RGB;

    png_ptr = png_create_write_struct( PNG_LIBPNG_VER_STRING,
                                       NULL, NULL, NULL );
    if ( png_ptr == NULL )
        return -1;

    info_ptr = png_create_info_struct( png_ptr );
    if ( info_ptr == NULL )
    {
        png_destroy_write_struct( &png_ptr, (png_infopp) NULL );
        return -1;
    }

    /* libpng's error path is a longjmp(); free everything we own and
     * return -1. */
    if ( setjmp( png_jmpbuf( png_ptr ) ) )
    {
        png_destroy_write_struct( &png_ptr, &info_ptr );
        free( row_ptrs );
        free( sink.buf );
        return -1;
    }

    png_set_write_fn( png_ptr, &sink, png_mem_write_cb, png_mem_flush_cb );
    /* Default compression level 6: good enough for screenshots, avoids
     * burning seconds of CPU on 4K captures that zlib -9 would trigger. */
    png_set_compression_level( png_ptr, 6 );

    png_set_IHDR( png_ptr, info_ptr,
                  (png_uint_32) width, (png_uint_32) height,
                  8, color_type,
                  PNG_INTERLACE_NONE,
                  PNG_COMPRESSION_TYPE_DEFAULT,
                  PNG_FILTER_TYPE_DEFAULT );

    png_write_info( png_ptr, info_ptr );

    /* Drop the alpha channel from the source if the caller asked for
     * RGB output. We avoid an extra allocation by flagging libpng to
     * do the 4→3 conversion itself. */
    if ( !alpha )
        png_set_filler( png_ptr, 0, PNG_FILLER_AFTER );

    row_bytes = (size_t) width * 4u;   /* source is always RGBA */
    row_ptrs  = (png_bytep *) malloc( (size_t) height * sizeof( png_bytep ) );
    if ( row_ptrs == NULL )
    {
        png_destroy_write_struct( &png_ptr, &info_ptr );
        free( sink.buf );
        return -1;
    }
    for ( i = 0; i < height; i++ )
        row_ptrs[i] = (png_bytep) ( rgba + (size_t) i * row_bytes );

    png_write_image( png_ptr, row_ptrs );
    png_write_end( png_ptr, info_ptr );

    png_destroy_write_struct( &png_ptr, &info_ptr );
    free( row_ptrs );

    if ( sink.err )
    {
        free( sink.buf );
        return -1;
    }

    *out_png = sink.buf;
    *out_len = sink.len;
    return 0;
}

/* ------------------------------------------------------------------
 * JPEG: in-memory encoder.
 *
 * libjpeg 6-b (the vendored copy in ext/JPEG/) does not ship the
 * jpeg_mem_dest() helper from later versions, so we wire a growable
 * byte buffer through a custom `jpeg_destination_mgr`. This keeps the
 * encoder working against both the vendored library and system
 * libjpeg-turbo without a version probe.
 *
 * Output format: baseline JPEG, 3-channel RGB. Alpha is discarded on
 * the way in (RGBA → RGB in-place per row). Quality is caller-provided
 * and clamped to [1, 100].
 * ------------------------------------------------------------------ */

typedef struct {
    struct jpeg_destination_mgr base;     /* must be first */
    unsigned char              *buf;
    size_t                      len;
    size_t                      cap;
    int                         err;      /* sticky */
} JpegMemSink;

/* 64 KB chunk — amortises realloc cost while keeping the waste per
 * frame bounded. */
#define JPEG_MEM_CHUNK ( (size_t) 64 * 1024 )

static void
jpeg_mem_init( j_compress_ptr cinfo )
{
    JpegMemSink *s = (JpegMemSink *) cinfo->dest;
    if ( s->buf == NULL )
    {
        s->buf = (unsigned char *) malloc( JPEG_MEM_CHUNK );
        if ( s->buf == NULL )
        {
            s->err = 1;
            ERREXIT( cinfo, JERR_OUT_OF_MEMORY );
            return;
        }
        s->cap = JPEG_MEM_CHUNK;
    }
    s->len = 0;
    s->base.next_output_byte = s->buf;
    s->base.free_in_buffer   = s->cap;
}

static boolean
jpeg_mem_empty( j_compress_ptr cinfo )
{
    JpegMemSink   *s = (JpegMemSink *) cinfo->dest;
    unsigned char *nb;
    size_t         new_cap;

    /* libjpeg calls this when the output buffer is full and expects us
     * to flush and hand it a fresh one. Grow the sink and keep going. */
    s->len = s->cap;
    new_cap = s->cap ? s->cap * 2 : JPEG_MEM_CHUNK;
    nb = (unsigned char *) realloc( s->buf, new_cap );
    if ( nb == NULL )
    {
        s->err = 1;
        ERREXIT( cinfo, JERR_OUT_OF_MEMORY );
        return FALSE;
    }
    s->buf = nb;
    s->cap = new_cap;
    s->base.next_output_byte = s->buf + s->len;
    s->base.free_in_buffer   = s->cap - s->len;
    return TRUE;
}

static void
jpeg_mem_term( j_compress_ptr cinfo )
{
    JpegMemSink *s = (JpegMemSink *) cinfo->dest;
    /* Final length = cap - whatever libjpeg didn't write. */
    s->len = s->cap - s->base.free_in_buffer;
}

/* Custom longjmp-based error manager so we can recover from bad input
 * without letting libjpeg exit() the process. */
struct jpeg_error_jmp {
    struct jpeg_error_mgr pub;
    jmp_buf               setjmp_buffer;
};

static void
jpeg_error_exit_longjmp( j_common_ptr cinfo )
{
    struct jpeg_error_jmp *err = (struct jpeg_error_jmp *) cinfo->err;
    longjmp( err->setjmp_buffer, 1 );
}

int
server_render_encode_jpeg( const unsigned char *rgba,
                           int width, int height, int quality,
                           unsigned char **out_jpeg,
                           size_t *out_len )
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_jmp       jerr;
    JpegMemSink                 sink;
    unsigned char              *rgb_row = NULL;
    size_t                      row_bytes_rgba;
    int                         y;

    if ( rgba == NULL || out_jpeg == NULL || out_len == NULL
         || width <= 0 || height <= 0 )
        return -1;

    *out_jpeg = NULL;
    *out_len  = 0;

    if ( quality <= 0 )
        quality = 85;
    if ( quality > 100 )
        quality = 100;

    memset( &sink, 0, sizeof( sink ) );
    sink.base.init_destination    = jpeg_mem_init;
    sink.base.empty_output_buffer = jpeg_mem_empty;
    sink.base.term_destination    = jpeg_mem_term;

    cinfo.err = jpeg_std_error( &jerr.pub );
    jerr.pub.error_exit = jpeg_error_exit_longjmp;

    if ( setjmp( jerr.setjmp_buffer ) )
    {
        jpeg_destroy_compress( &cinfo );
        free( rgb_row );
        free( sink.buf );
        return -1;
    }

    jpeg_create_compress( &cinfo );
    cinfo.dest = (struct jpeg_destination_mgr *) &sink;

    cinfo.image_width      = (JDIMENSION) width;
    cinfo.image_height     = (JDIMENSION) height;
    cinfo.input_components = 3;
    cinfo.in_color_space   = JCS_RGB;

    jpeg_set_defaults( &cinfo );
    jpeg_set_quality( &cinfo, quality, TRUE );

    jpeg_start_compress( &cinfo, TRUE );

    row_bytes_rgba = (size_t) width * 4u;
    rgb_row = (unsigned char *) malloc( (size_t) width * 3u );
    if ( rgb_row == NULL )
    {
        jpeg_destroy_compress( &cinfo );
        free( sink.buf );
        return -1;
    }

    for ( y = 0; y < height; y++ )
    {
        const unsigned char *src = rgba + (size_t) y * row_bytes_rgba;
        unsigned char       *dst = rgb_row;
        JSAMPROW             row = (JSAMPROW) rgb_row;
        int                  x;
        for ( x = 0; x < width; x++ )
        {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            src += 4;
            dst += 3;
        }
        (void) jpeg_write_scanlines( &cinfo, &row, 1 );
    }

    jpeg_finish_compress( &cinfo );
    jpeg_destroy_compress( &cinfo );
    free( rgb_row );

    if ( sink.err )
    {
        free( sink.buf );
        return -1;
    }

    *out_jpeg = sink.buf;
    *out_len  = sink.len;
    return 0;
}

/* Helper for push_jpeg_frame: monotonic-ish wall-clock seconds since
 * Unix epoch as a double, for the JSON sub-header's `rendered_at`
 * field. CLOCK_REALTIME is fine here — this field is a timestamp, not
 * a latency measurement. */
static double
now_epoch_seconds( void )
{
    struct timespec ts;
    if ( clock_gettime( CLOCK_REALTIME, &ts ) != 0 )
        return 0.0;
    return (double) ts.tv_sec + (double) ts.tv_nsec * 1e-9;
}

static double
elapsed_ms( const struct timespec *start )
{
    struct timespec now;
    if ( clock_gettime( CLOCK_MONOTONIC, &now ) != 0 )
        return 0.0;
    return ( now.tv_sec - start->tv_sec ) * 1000.0
         + ( now.tv_nsec - start->tv_nsec ) / 1.0e6;
}

int
server_render_push_jpeg_frame( Analysis *analy, int quality )
{
    unsigned char  *rgba    = NULL;
    int             w       = 0;
    int             h       = 0;
    unsigned char  *jpeg    = NULL;
    size_t          jpeg_len = 0;
    struct timespec enc_start;
    double          encode_ms;
    unsigned long long seq;
    cJSON          *hdr;
    char           *hdr_txt;
    int             rc;

    if ( !server_has_binary_transport() )
        return -1;

    if ( quality <= 0 )
        quality = 85;

    if ( server_render_capture_rgba( analy, &rgba, &w, &h ) != 0
         || rgba == NULL )
        return -1;

    if ( clock_gettime( CLOCK_MONOTONIC, &enc_start ) != 0 )
    {
        enc_start.tv_sec  = 0;
        enc_start.tv_nsec = 0;
    }

    if ( server_render_encode_jpeg( rgba, w, h, quality,
                                    &jpeg, &jpeg_len ) != 0
         || jpeg == NULL )
    {
        free( rgba );
        return -1;
    }
    free( rgba );

    encode_ms = elapsed_ms( &enc_start );
    seq       = server_render_next_frame_seq();

    hdr = cJSON_CreateObject();
    if ( hdr == NULL )
    {
        free( jpeg );
        return -1;
    }
    cJSON_AddNumberToObject( hdr, "w",           (double) w );
    cJSON_AddNumberToObject( hdr, "h",           (double) h );
    cJSON_AddNumberToObject( hdr, "seq",         (double) seq );
    cJSON_AddStringToObject( hdr, "fmt",         "jpeg" );
    cJSON_AddNumberToObject( hdr, "bytes",       (double) jpeg_len );
    cJSON_AddNumberToObject( hdr, "quality",     (double) quality );
    cJSON_AddNumberToObject( hdr, "encode_ms",   encode_ms );
    cJSON_AddNumberToObject( hdr, "rendered_at", now_epoch_seconds() );

    hdr_txt = cJSON_PrintUnformatted( hdr );
    cJSON_Delete( hdr );

    /* subtype=0x01 (frame), codec=0x01 (jpeg), flags=0x04 (last — the
     * frame is a complete JPEG, no continuation chunks in v0). */
    rc = server_emit_binary_frame( 0x01, 0x01, 0x04,
                                   hdr_txt, jpeg, jpeg_len );
    free( hdr_txt );
    free( jpeg );
    return rc;
}

int
server_render_resize_viewport( int w, int h )
{
    unsigned char *new_buf;
    unsigned char *old_buf;
    size_t         sz;

    if ( w < 1 || h < 1
         || w > SERVER_RENDER_VIEWPORT_MAX
         || h > SERVER_RENDER_VIEWPORT_MAX )
        return 1;

    if ( OSMesa_ctx == NULL )
        return -1;

    sz = (size_t) w * (size_t) h * 4u;
    new_buf = (unsigned char *) malloc( sz );
    if ( new_buf == NULL )
        return -1;

    if ( OSMesaMakeCurrent( OSMesa_ctx, new_buf,
                            GL_UNSIGNED_BYTE, w, h ) == GL_FALSE )
    {
        free( new_buf );
        return -1;
    }

    old_buf = s_osmesa_buffer;
    s_osmesa_buffer = new_buf;
    /* Only free buffers we allocated ourselves; the initial one from
     * OffscreenContext() has no tracked pointer and would leak here
     * regardless. */
    if ( old_buf != NULL )
        free( old_buf );

    /* Bring the legacy GL viewport + v_win state in line with the new
     * framebuffer dimensions. set_mesh_view() reads GL_VIEWPORT and
     * rewrites v_win->vp_width / vp_height + the projection matrix. */
    glViewport( 0, 0, (GLint) w, (GLint) h );
    set_mesh_view();

    return 0;
}

#endif /* GRIZ_SERVER_BUILD */
