/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPLAYER_MP_IMAGE_H
#define MPLAYER_MP_IMAGE_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "core/mp_msg.h"
#include "csputils.h"

// Minimum stride alignment in pixels
#define MP_STRIDE_ALIGNMENT 32

//--------- color info (filled by mp_image_setfmt() ) -----------
// set if number of planes > 1
#define MP_IMGFLAG_PLANAR 0x100
// set if it's YUV colorspace
#define MP_IMGFLAG_YUV 0x200
// set if it's swapped (BGR or YVU) plane/byteorder
#define MP_IMGFLAG_SWAPPED 0x400
// set if you want memory for palette allocated and managed by vf_get_image etc.
#define MP_IMGFLAG_RGB_PALETTE 0x800


// set if buffer is allocated (used in destination images):
#define MP_IMGFLAG_ALLOCATED 0x4000

#define MP_MAX_PLANES	4

#define MP_IMGFIELD_ORDERED 0x01
#define MP_IMGFIELD_TOP_FIRST 0x02
#define MP_IMGFIELD_REPEAT_FIRST 0x04
#define MP_IMGFIELD_TOP 0x08
#define MP_IMGFIELD_BOTTOM 0x10
#define MP_IMGFIELD_INTERLACED 0x20

/* Memory management:
 * - mp_image is a light-weight reference to the actual image data (pixels).
 *   The actual image data is reference counted and can outlive mp_image
 *   allocations. mp_image references can be created with mp_image_new_ref()
 *   and free'd with talloc_free() (the helpers mp_image_setrefp() and
 *   mp_image_unrefp() can also be used). The actual image data is free'd when
 *   the last mp_image reference to it is free'd.
 * - Each mp_image has a clear owner. The owner can do anything with it, such
 *   as changing mp_image fields. Instead of making ownership ambiguous by
 *   sharing a mp_image reference, new references should be created.
 * - Write access to the actual image data is allowed only after calling
 *   mp_image_make_writeable(), or if mp_image_is_writeable() returns true.
 *   Conceptually, images can be changed by their owner only, and copy-on-write
 *   is used to ensure that other references do not see any changes to the
 *   image data. mp_image_make_writeable() will do that copy if required.
 */
typedef struct mp_image {
    unsigned int flags;
    unsigned char bpp;  // bits/pixel. NOT depth! for RGB it will be n*8
    unsigned int imgfmt;
    int w,h;  // visible dimensions
    int display_w,display_h; // if set (!= 0), anamorphic size
    uint8_t *planes[MP_MAX_PLANES];
    int stride[MP_MAX_PLANES];
    char * qscale;
    int qstride;
    int pict_type; // 0->unknown, 1->I, 2->P, 3->B
    int fields;
    int qscale_type; // 0->mpeg1/4/h263, 1->mpeg2
    int num_planes;
    /* these are only used by planar formats Y,U(Cb),V(Cr) */
    int chroma_width;
    int chroma_height;
    int chroma_x_shift; // horizontal
    int chroma_y_shift; // vertical
    enum mp_csp colorspace;
    enum mp_csp_levels levels;
    /* only inside filter chain */
    double pts;
    /* memory management */
    struct m_refcount *refcount;
    /* for private use */
    void* priv;
} mp_image_t;

#define alloc_mpi(w, h, fmt) mp_image_alloc(fmt, w, h)
#define free_mp_image talloc_free
#define new_mp_image mp_image_new_empty
#define copy_mpi mp_image_copy

struct mp_image *mp_image_alloc(unsigned int fmt, int w, int h);
void mp_image_copy(struct mp_image *dmpi, struct mp_image *mpi);
void mp_image_copy_attributes(struct mp_image *dmpi, struct mp_image *mpi);
struct mp_image *mp_image_new_copy(struct mp_image *img);
struct mp_image *mp_image_new_ref(struct mp_image *img);
bool mp_image_is_writeable(struct mp_image *img);
void mp_image_make_writeable(struct mp_image *img);
void mp_image_setrefp(struct mp_image **p_img, struct mp_image *new_value);
void mp_image_unrefp(struct mp_image **p_img);

void mp_image_set_size(struct mp_image *mpi, int w, int h);
void mp_image_set_display_size(struct mp_image *mpi, int dw, int dh);

struct mp_image *mp_image_new_empty(int w, int h);
void mp_image_setfmt(mp_image_t* mpi,unsigned int out_fmt);
void mp_image_alloc_planes(struct mp_image *mpi);
void mp_image_steal_data(struct mp_image *dst, struct mp_image *src);

struct mp_image *mp_image_new_custom_ref(struct mp_image *img, void *arg,
                                         void (*free)(void *arg));

struct mp_image *mp_image_new_external_ref(struct mp_image *img, void *arg,
                                           void (*ref)(void *arg),
                                           void (*unref)(void *arg),
                                           bool (*is_unique)(void *arg));

enum mp_csp mp_image_csp(struct mp_image *img);
enum mp_csp_levels mp_image_levels(struct mp_image *img);

struct mp_csp_details;
void mp_image_set_colorspace_details(struct mp_image *image,
                                     struct mp_csp_details *csp);

// this macro requires img_format.h to be included too:
#define MP_IMAGE_PLANAR_BITS_PER_PIXEL_ON_PLANE(mpi, p) \
    (IMGFMT_IS_YUVP16((mpi)->imgfmt) ? 16 : 8)
#define MP_IMAGE_BITS_PER_PIXEL_ON_PLANE(mpi, p) \
    (((mpi)->flags & MP_IMGFLAG_PLANAR) \
        ? MP_IMAGE_PLANAR_BITS_PER_PIXEL_ON_PLANE(mpi, p) \
        : (mpi)->bpp)
#define MP_IMAGE_BYTES_PER_ROW_ON_PLANE(mpi, p) \
    ((MP_IMAGE_BITS_PER_PIXEL_ON_PLANE(mpi, p) * ((mpi)->w >> (p ? mpi->chroma_x_shift : 0)) + 7) / 8)

#endif /* MPLAYER_MP_IMAGE_H */
