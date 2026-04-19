#ifndef __GL_IMAGE_H__
#define __GL_IMAGE_H__
#ifdef __cplusplus
extern "C" {
#endif


/*
 *      Defines for image files . . . .
 *
 *                      Paul Haeberli - 1984
 *      Look in /usr/people/4Dgifts/iristools/imgtools for example code!
 *
 */

#include <stdio.h>
#include <stdint.h>

/*
 * Portability types for 64-bit architectures.
 *
 * SIGNED_4BYTE / UNSIGNED_4BYTE are used for on-disk header fields
 * (min, max, wastebytes, colormap, rowstart[], rowsize[]) and are
 * baked into the offset arithmetic inside cvtimage() in open.c
 * (e.g. `cvtlongs(buffer+26, 4)` assumes these are exactly 4 bytes).
 * The default `unsigned long` typedef below produces 8-byte types on
 * 64-bit Linux and causes cvtimage() to write past the end of the
 * IMAGE struct, corrupting the malloc heap and aborting the next
 * free() in iclose(). Fix: pin to int32_t / uint32_t on Linux.
 */
#ifdef __alpha
#define ULONG_TYPE
#define UNSIGNED_4BYTE unsigned int
#define SIGNED_4BYTE int
#define DO_REVERSE 1
#endif

#ifdef cray
#define ULONG_TYPE
#define UNSIGNED_4BYTE unsigned int
#define SIGNED_4BYTE int
#define DO_REVERSE 0
#endif

#ifdef __linux
#define ULONG_TYPE
#define UNSIGNED_4BYTE uint32_t
#define SIGNED_4BYTE   int32_t
#define DO_REVERSE 1
#define _IOEOF          0020    /* EOF reached on read */
#define _IOERR          0040    /* I/O error from system */
#define _IOREAD         0001    /* currently reading */
#define _IOWRT          0002    /* currently writing */
#define _IORW           0200    /* opened for reading and writing */
#endif

#ifndef DO_REVERSE
#define DO_REVERSE 1
#define _IOEOF          0020    /* EOF reached on read */
#define _IOERR          0040    /* I/O error from system */
#define _IOREAD         0001    /* currently reading */
#define _IOWRT          0002    /* currently writing */
#define _IORW           0200    /* opened for reading and writing */
#endif

#ifndef ULONG_TYPE
#define UNSIGNED_4BYTE unsigned long
#define SIGNED_4BYTE long
#endif

#ifndef DO_REVERSE
#define DO_REVERSE 0
#endif

#define IMAGIC  0732

/* colormap of images */
#define CM_NORMAL               0       /* file contains rows of values which 
                                         * are either RGB values (zsize == 3) 
                                         * or greyramp values (zsize == 1) */
#define CM_DITHERED             1
#define CM_SCREEN               2       /* file contains data which is a screen
                                         * image; getrow returns buffer which 
                                         * can be displayed directly with 
                                         * writepixels */
#define CM_COLORMAP             3       /* a colormap file */

#define TYPEMASK                0xff00
#define BPPMASK                 0x00ff
#define ITYPE_VERBATIM          0x0000
#define ITYPE_RLE               0x0100
#define ISRLE(type)             (((type) & 0xff00) == ITYPE_RLE)
#define ISVERBATIM(type)        (((type) & 0xff00) == ITYPE_VERBATIM)
#define BPP(type)               ((type) & BPPMASK)
#define RLE(bpp)                (ITYPE_RLE | (bpp))
#define VERBATIM(bpp)           (ITYPE_VERBATIM | (bpp))
#define IBUFSIZE(pixels)        ((pixels+(pixels>>6))<<2)
#define RLE_NOP                 0x00

#define ierror(p)               (((p)->flags&_IOERR)!=0)
#define ifileno(p)              ((p)->file)
#define getpix(p)               (--(p)->cnt>=0 ? *(p)->ptr++ : ifilbuf(p))
#define putpix(p,x)             (--(p)->cnt>=0 \
                                    ? ((int)(*(p)->ptr++=(unsigned)(x))) \
                                    : iflsbuf(p,(unsigned)(x)))

typedef struct {
    unsigned short      imagic;         /* stuff saved on disk . . */
    unsigned short      type;
    unsigned short      dim;
    unsigned short      xsize;
    unsigned short      ysize;
    unsigned short      zsize;
    UNSIGNED_4BYTE      min;
    UNSIGNED_4BYTE      max;
    UNSIGNED_4BYTE      wastebytes;     
    char                name[80];
    UNSIGNED_4BYTE      colormap;

    long                file;           /* stuff used in core only */
    unsigned short      flags;
    short               dorev;
    short               x;
    short               y;
    short               z;
    short               cnt;
    unsigned short      *ptr;
    unsigned short      *base;
    unsigned short      *tmpbuf;
    UNSIGNED_4BYTE      offset;
    UNSIGNED_4BYTE      rleend;         /* for rle images */
    UNSIGNED_4BYTE      *rowstart;      /* for rle images */
    SIGNED_4BYTE        *rowsize;       /* for rle images */
} IMAGE;

IMAGE *icreate();
/*
 * IMAGE *iopen(char *file, char *mode, unsigned int type, unsigned int dim,
 *              unsigned int xsize, unsigned int ysize, unsigned int zsize);
 * IMAGE *fiopen(int f, char *mode, unsigned int type, unsigned int dim,
 *              unsigned int xsize, unsigned int ysize, unsigned int zsize);
 *
 * ...while iopen and fiopen can take an extended set of parameters, the 
 * last five are optional, so a more correct prototype would be:
 *
 * IMAGE *iopen(char *file, char *mode, ...);
 * IMAGE *fiopen(int f, char *mode, ...);
 * 
 * unsigned short *ibufalloc(IMAGE *image);
 * int ifilbuf(IMAGE *image);
 * int iflush(IMAGE *image);
 * unsigned int iflsbuf(IMAGE *image, unsigned int c);
 * void isetname(IMAGE *image, char *name);
 * void isetcolormap(IMAGE *image, int colormap);
 * int iclose(IMAGE *image);
 * 
 * int putrow(IMAGE *image, unsigned short *buffer, unsigned int y, unsigned int z);
 * int getrow(IMAGE *image, unsigned short *buffer, unsigned int y, unsigned int z);
 * 
 */

/* IMAGE *iopen(); */
extern IMAGE *iopen(char *file, char *mode, unsigned int type, unsigned int dim,
                    unsigned int xsize, unsigned int ysize, unsigned int zsize);
extern int iclose(IMAGE *image);
extern void i_seterror( void (*)() );
IMAGE *icreate();
unsigned short *ibufalloc();
extern int putrow(IMAGE *image, unsigned short *buffer, unsigned int y, 
                  unsigned int z);

#define IMAGEDEF                /* for backwards compatibility */
#ifdef __cplusplus
}
#endif
#endif  /* !__GL_IMAGE_H__ */
