/*
 * server_render.c - see server_render.h for the contract.
 */

#ifdef GRIZ_SERVER_BUILD

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include <GL/gl.h>

#include <png.h>

#include "viewer.h"
#include "draw.h"
#include "server_render.h"

/* Module-level frame_seq counter. Protected implicitly by the single-
 * threaded v0 dispatch loop; when the three-thread split lands
 * (planning/UI.md Phase 2), this counter moves to the render thread. */
static unsigned long long s_frame_seq = 0;

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

#endif /* GRIZ_SERVER_BUILD */
