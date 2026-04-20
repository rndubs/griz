/*
 * server_render.h - Offscreen rendering + in-memory image encoding for
 * griz-server.
 *
 * Phase 3 of planning/UI.md. The RPC transport emits rendered frames
 * and inline screenshots as kind=0x02 binary frames (02-protocol.md §3,
 * 05-rendering-and-streaming.md §§3, 7.1). This TU owns:
 *
 *   - A render-and-capture primitive that drives `analy->update_display`,
 *     fences with glFinish, and pulls RGBA pixels via screen_to_memory.
 *   - An in-memory PNG encoder used by the inline-screenshot path. We
 *     reimplement it here (rather than extend the filesystem-based
 *     write_PNG_file in draw.c) so the server binary's PNG path stays
 *     transport-framed and independent of the interactive build's PNG
 *     disk-write code — matching the "zero intrusive changes to
 *     draw.c" invariant in 05 §8.
 *   - A monotonic frame-sequence counter (02 §4.2).
 *
 * All helpers are synchronous and single-threaded; the render/command/
 * I-O thread split (03-server.md §2.2) is deferred past v0.
 */

#ifndef SERVER_RENDER_H
#define SERVER_RENDER_H

#ifdef GRIZ_SERVER_BUILD

#include <stddef.h>

#include "viewer.h"

/* Render the current view into the OSMesa framebuffer and capture the
 * pixels as RGBA8. Returns 0 on success and -1 on allocation failure.
 *
 *   analy    : analysis context; must be non-NULL. `analy->update_display`
 *              is invoked, so any dirty-view state is flushed first.
 *   out_rgba : receives a newly-allocated, caller-owned buffer of
 *              W * H * 4 bytes in top-to-bottom row order after this
 *              function rewrites the OpenGL bottom-up layout. Free with
 *              free().
 *   out_w    : receives the rendered viewport width (pixels).
 *   out_h    : receives the rendered viewport height (pixels).
 *
 * The caller is expected to run on the same thread that owns the
 * OSMesa context (i.e. the dispatch thread, matching the single-thread
 * GL invariant I6 in 01-architecture.md §6).
 */
int server_render_capture_rgba( Analysis *analy,
                                unsigned char **out_rgba,
                                int *out_w,
                                int *out_h );

/* Encode an RGBA8 buffer into a PNG byte stream. The returned buffer is
 * newly allocated and must be freed by the caller.
 *
 *   rgba         : W * H * 4 bytes, top-to-bottom row order.
 *   width,height : image dimensions in pixels.
 *   alpha        : non-zero to emit PNG_COLOR_TYPE_RGB_ALPHA (4-channel);
 *                  zero to drop alpha and emit PNG_COLOR_TYPE_RGB.
 *   out_png      : receives the allocated PNG byte stream.
 *   out_len      : receives the length in bytes.
 *
 * Returns 0 on success, -1 on libpng / allocation failure.
 */
int server_render_encode_png( const unsigned char *rgba,
                              int width, int height, int alpha,
                              unsigned char **out_png,
                              size_t *out_len );

/* Encode an RGBA8 buffer into a baseline JPEG byte stream. Alpha is
 * discarded (JPEG has no alpha channel).
 *
 *   rgba         : W * H * 4 bytes, top-to-bottom row order.
 *   width,height : image dimensions in pixels.
 *   quality      : 1..100 (85 is the MVP default per
 *                  05-rendering-and-streaming.md §4.1).
 *   out_jpeg     : receives the allocated JPEG byte stream.
 *   out_len      : receives the length in bytes.
 *
 * Returns 0 on success, -1 on libjpeg / allocation failure.
 */
int server_render_encode_jpeg( const unsigned char *rgba,
                               int width, int height, int quality,
                               unsigned char **out_jpeg,
                               size_t *out_len );

/* Render + encode the current view as a JPEG and push it as a
 * `kind=0x02` subtype=0x01 codec=0x01 binary frame on the installed
 * binary emitter. The frame's JSON sub-header carries
 *   { w, h, seq, fmt:"jpeg", bytes, quality, encode_ms, rendered_at }
 * per 05-rendering-and-streaming.md §4.1.
 *
 * No-op if the current transport does not carry binary frames (stdio);
 * returns -1 in that case. Otherwise 0 on success, -1 on render /
 * encode / emit failure. `quality` is clamped to [1, 100]; pass 0 to
 * use the MVP default (85). */
int server_render_push_jpeg_frame( Analysis *analy, int quality );

/* Return the next monotonic frame-sequence counter. Each successful
 * capture or inline-screenshot emission burns one seq; clients infer
 * drops from gaps (02-protocol.md §4.2). */
unsigned long long server_render_next_frame_seq( void );

/* Resize the OSMesa viewport to (w, h).
 *
 * Allocates a fresh RGBA8 backing buffer, rebinds the existing
 * OSMesa_ctx to it via OSMesaMakeCurrent(), and propagates the new
 * dimensions into the legacy GL viewport state (`glViewport()` +
 * `set_mesh_view()` so `v_win->vp_width/vp_height` update). The caller
 * is expected to trigger a redraw after this returns — typically by
 * letting `analy->update_display()` run on the next render.
 *
 * Size cap: 05-rendering-and-streaming.md §2.1 — (w, h) each ≤ 4096.
 * Returns 0 on success, -1 on allocation failure, 1 on invalid size
 * (< 1 or > 4096 on either axis). The previous backing buffer
 * allocated inside server_render_resize_viewport() is freed; the
 * initial buffer set up by OffscreenContext() at startup is not
 * tracked and leaks on the first resize (one-time, <=64 MiB).
 */
int server_render_resize_viewport( int w, int h );

#endif /* GRIZ_SERVER_BUILD */

#endif /* SERVER_RENDER_H */
