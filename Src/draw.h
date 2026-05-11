/* $Id$ */
/*
 * draw.h - Definitions for display routines.
 *
 *      Donald J. Dovey
 *      Lawrence Livermore National Laboratory
 *      Oct 23 1991
 *
 ************************************************************************
 * Modifications:
 *
 *  I. R. Corey - Jul 25, 2007: Added feature to plot disabled materials in
 *                greyscale.
 *                See SRC#476.
 ************************************************************************
 */

#ifndef DRAW_H
#define DRAW_H

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glx.h>
#include "misc.h"
#include "geometric.h"

#define CMAP_SIZE (256)

typedef GLfloat GLVec4[4];

typedef struct
{

    /*
     * Note: For mesh materials, property_array_size is only implemented
     * to give the current size
     * of the allocated property arrays.  It should not be used generically
     * as the number of mesh materials because neither create_mat_prop_arrays()
     * nor extend_mat_prop_arrays() synchronize everything that depends on
     * material quantity (such as p_mesh->hide_mtl...).
     */
    int      current_index;
    int      property_array_size;
    GLVec4  *ambient;
    GLVec4  *diffuse;
    GLVec4  *specular;
    GLfloat *shininess;
    GLVec4  *emission;
    GLfloat *gslevel;
}  Color_property;

typedef struct
{
    int win_x;
    int win_y;
    GLsizei vp_width;
    GLsizei vp_height;
    float aspect_correct;         /* Aspect correction for video output. */

    Bool_type orthographic;
    float cam_angle;
    float look_from[3];
    float look_at[3];
    float look_up[3];
    GLdouble near;
    GLdouble far;

    Transf_mat rot_mat;
    float trans[3];
    float scale[3];

    float bbox_trans[3];
    float bbox_scale;

#ifdef OLD_MATL_PROPS
    GLfloat matl_ambient[MAX_MATERIALS][4];
    GLfloat matl_diffuse[MAX_MATERIALS][4];
    GLfloat matl_specular[MAX_MATERIALS][4];
    GLfloat matl_shininess[MAX_MATERIALS];
    GLfloat matl_emission[MAX_MATERIALS][4];
#endif
#ifndef OLD_MATL_PROPS
    /*
     * Note: property_array_size is only implemented to give the current size
     * of the allocated property arrays.  It should not be used generically
     * as the number of mesh materials because neither create_mat_prop_arrays()
     * nor extend_mat_prop_arrays() synchronize everything that depends on
     * material quantity (such as p_mesh->hide_mtl...).
     */
    int property_array_size;
    GLVec4 *matl_ambient;
    GLVec4 *matl_diffuse;
    GLVec4 *matl_specular;
    GLfloat *matl_shininess;
    GLVec4 *matl_emission;
#endif

    Color_property *current_color_property;
    Color_property mesh_materials;
    Color_property surfaces;
    Color_property particles;

    GLfloat backgrnd_color[3];
    GLfloat foregrnd_color[3];
    GLfloat text_color[3];
    GLfloat mesh_color[3];
    GLfloat edge_color[3];
    GLfloat contour_color[3];
    GLfloat hilite_color[3];
    GLfloat select_color[3];
    GLfloat vector_color[3];
    GLfloat vector_hd_color[3];
    GLfloat cmap_min_color[3];   /* Last five tied to a colormap. */
    GLfloat cmap_max_color[3];
    GLfloat rmin_color[3];
    GLfloat rmax_color[3];
    GLfloat colormap[CMAP_SIZE][3];
    GLfloat initial_colormap[CMAP_SIZE][3];

    Bool_type lighting;
    Bool_type light_active[6];
    float light_pos[6][4];

    char vid_title[4][80];

} View_win_obj;

extern View_win_obj *v_win;


/*
 * Some useful RGB triples.
 */
extern GLfloat black[];
extern GLfloat white[];
extern GLfloat red[];
extern GLfloat green[];
extern GLfloat blue[];
extern GLfloat magenta[];
extern GLfloat cyan[];
extern GLfloat yellow[];

/* Picking (ID-buffer) draw mode — planning/ui-design/06-picking-and-queries.md
 * §3.1 MVP. When enabled, a small set of per-primitive draw loops (hexs,
 * tets, quads, tris, nodes) replace their fragment colours with a packed
 * RGBA that encodes (element/node id) in RGB and a class-tag byte in A, so
 * a single glReadPixels under the cursor decodes back to a pick hit.
 * Transparent materials (diffuse alpha < 1) are skipped in the ID pass
 * per §3.3. Normal rendering is unaffected when the flag is off. */
extern void griz_set_draw_ids_mode( int enabled );
extern int  griz_draw_ids_mode_active( void );

/* Class-tag values packed into the alpha channel of the ID-buffer pixel.
 * Kept small so the 0..255 → 0.0..1.0 float round-trip survives exact
 * readback via GL_RGBA / GL_UNSIGNED_BYTE. Tag 0 is reserved for a miss
 * (ID buffer is cleared to 0x00000000 before the pick render pass). */
#define GRIZ_ID_TAG_MISS  0
#define GRIZ_ID_TAG_NODE  1
#define GRIZ_ID_TAG_HEX   2
#define GRIZ_ID_TAG_TET   3
#define GRIZ_ID_TAG_QUAD  4
#define GRIZ_ID_TAG_TRI   5

#endif /* DRAW_H */
