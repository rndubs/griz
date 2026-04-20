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

/* Return the next monotonic frame-sequence counter. Each successful
 * capture or inline-screenshot emission burns one seq; clients infer
 * drops from gaps (02-protocol.md §4.2). */
unsigned long long server_render_next_frame_seq( void );

#endif /* GRIZ_SERVER_BUILD */

#endif /* SERVER_RENDER_H */
