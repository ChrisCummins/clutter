/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *
 * Authors:
 *  Matthew Allum  <mallum@openedhand.com>
 *  Neil Roberts   <neil@linux.intel.com>
 *  Robert Bragg   <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl.h"
#include "cogl-debug.h"
#include "cogl-internal.h"
#include "cogl-util.h"
#include "cogl-bitmap.h"
#include "cogl-bitmap-private.h"
#include "cogl-texture-private.h"
#include "cogl-texture-2d-sliced-private.h"
#include "cogl-texture-driver.h"
#include "cogl-context.h"
#include "cogl-handle.h"
#include "cogl-spans.h"
#include "cogl-journal-private.h"
#include "cogl-material-opengl-private.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>

static void _cogl_texture_2d_sliced_free (CoglTexture2DSliced *tex_2ds);

COGL_TEXTURE_INTERNAL_DEFINE (Texture2DSliced, texture_2d_sliced);

static const CoglTextureVtable cogl_texture_2d_sliced_vtable;

/* To differentiate between texture coordinates of a specific, real, slice
 * texture and the texture coordinates of the composite, sliced texture, the
 * coordinates of the sliced texture are called "virtual" coordinates and the
 * coordinates of slices are called "slice" coordinates. */
/* This function lets you iterate all the slices that lie within the given
 * virtual coordinates of the parent sliced texture. */
/* Note: no guarantee is given about the order in which the slices will be
 * visited */
static void
_cogl_texture_2d_sliced_foreach_sub_texture_in_region (
                                       CoglTexture *tex,
                                       float virtual_tx_1,
                                       float virtual_ty_1,
                                       float virtual_tx_2,
                                       float virtual_ty_2,
                                       CoglTextureSliceCallback callback,
                                       void *user_data)
{
  CoglTexture2DSliced *tex_2ds = COGL_TEXTURE_2D_SLICED (tex);
  float width = tex_2ds->width;
  float height = tex_2ds->height;
  CoglSpanIter iter_x;
  CoglSpanIter iter_y;

  /* Slice spans are stored in denormalized coordinates, and this is what
   * the _cogl_span_iter_* funcs expect to be given, so we scale the given
   * virtual coordinates by the texture size to denormalize.
   */
  /* XXX: I wonder if it's worth changing how we store spans so we can avoid
   * the need to denormalize here */
  virtual_tx_1 *= width;
  virtual_ty_1 *= height;
  virtual_tx_2 *= width;
  virtual_ty_2 *= height;

  /* Iterate the y axis of the virtual rectangle */
  for (_cogl_span_iter_begin (&iter_y,
                              tex_2ds->slice_y_spans,
                              height,
                              virtual_ty_1,
                              virtual_ty_2);
       !_cogl_span_iter_end (&iter_y);
       _cogl_span_iter_next (&iter_y))
    {
      float y_intersect_start = iter_y.intersect_start;
      float y_intersect_end = iter_y.intersect_end;
      float slice_ty1;
      float slice_ty2;

      /* Discard slices out of rectangle early */
      if (!iter_y.intersects)
        continue;

      if (iter_y.flipped)
        {
          y_intersect_start = iter_y.intersect_end;
          y_intersect_end = iter_y.intersect_start;
        }

      /* Localize slice texture coordinates */
      slice_ty1 = y_intersect_start - iter_y.pos;
      slice_ty2 = y_intersect_end - iter_y.pos;

      if (tex_2ds->gl_target == GL_TEXTURE_2D)
	{
	  /* Normalize slice texture coordinates */
	  slice_ty1 /= iter_y.span->size;
	  slice_ty2 /= iter_y.span->size;
	}

      /* Iterate the x axis of the virtual rectangle */
      for (_cogl_span_iter_begin (&iter_x,
                                  tex_2ds->slice_x_spans,
                                  width,
                                  virtual_tx_1,
                                  virtual_tx_2);
	   !_cogl_span_iter_end (&iter_x);
	   _cogl_span_iter_next (&iter_x))
        {
          float slice_coords[4];
          float virtual_coords[4];
          float x_intersect_start = iter_x.intersect_start;
          float x_intersect_end = iter_x.intersect_end;
          float slice_tx1;
          float slice_tx2;
          GLuint gl_handle;

	  /* Discard slices out of rectangle early */
	  if (!iter_x.intersects)
            continue;

          if (iter_x.flipped)
            {
              x_intersect_start = iter_x.intersect_end;
              x_intersect_end = iter_x.intersect_start;
            }

	  /* Localize slice texture coordinates */
          slice_tx1 = x_intersect_start - iter_x.pos;
          slice_tx2 = x_intersect_end - iter_x.pos;

	  /* Pluck out opengl texture object for this slice */
	  gl_handle = g_array_index (tex_2ds->slice_gl_handles, GLuint,
				     iter_y.index * iter_x.array->len +
				     iter_x.index);

	  if (tex_2ds->gl_target == GL_TEXTURE_2D)
	    {
	      /* Normalize slice texture coordinates */
	      slice_tx1 /= iter_x.span->size;
	      slice_tx2 /= iter_x.span->size;
	    }

          slice_coords[0] = slice_tx1;
          slice_coords[1] = slice_ty1;
          slice_coords[2] = slice_tx2;
          slice_coords[3] = slice_ty2;

          virtual_coords[0] = x_intersect_start / width;
          virtual_coords[1] = y_intersect_start / height;
          virtual_coords[2] = x_intersect_end / width;
          virtual_coords[3] = y_intersect_end / height;

          callback (tex,
                    gl_handle,
                    tex_2ds->gl_target,
                    slice_coords,
                    virtual_coords,
                    user_data);
	}
    }
}

static guint8 *
_cogl_texture_2d_sliced_allocate_waste_buffer (CoglTexture2DSliced *tex_2ds,
                                               CoglPixelFormat format)
{
  CoglSpan *last_x_span;
  CoglSpan *last_y_span;
  guint8   *waste_buf = NULL;

  /* If the texture has any waste then allocate a buffer big enough to
     fill the gaps */
  last_x_span = &g_array_index (tex_2ds->slice_x_spans, CoglSpan,
                                tex_2ds->slice_x_spans->len - 1);
  last_y_span = &g_array_index (tex_2ds->slice_y_spans, CoglSpan,
                                tex_2ds->slice_y_spans->len - 1);
  if (last_x_span->waste > 0 || last_y_span->waste > 0)
    {
      int bpp = _cogl_get_format_bpp (format);
      CoglSpan  *first_x_span
        = &g_array_index (tex_2ds->slice_x_spans, CoglSpan, 0);
      CoglSpan  *first_y_span
        = &g_array_index (tex_2ds->slice_y_spans, CoglSpan, 0);
      unsigned int right_size = first_y_span->size * last_x_span->waste;
      unsigned int bottom_size = first_x_span->size * last_y_span->waste;

      waste_buf = g_malloc (MAX (right_size, bottom_size) * bpp);
    }

  return waste_buf;
}

static gboolean
_cogl_texture_2d_sliced_upload_to_gl (CoglTexture2DSliced *tex_2ds,
                                      CoglBitmap          *bmp,
                                      GLenum               gl_intformat,
                                      GLenum               gl_format,
                                      GLenum               gl_type)
{
  CoglSpan        *x_span;
  CoglSpan        *y_span;
  GLuint           gl_handle;
  int              bpp;
  int              x, y;
  guint8          *waste_buf;
  CoglPixelFormat  bmp_format;

  bmp_format = _cogl_bitmap_get_format (bmp);
  bpp = _cogl_get_format_bpp (bmp_format);

  waste_buf = _cogl_texture_2d_sliced_allocate_waste_buffer (tex_2ds,
                                                             bmp_format);

  /* Iterate vertical slices */
  for (y = 0; y < tex_2ds->slice_y_spans->len; ++y)
    {
      y_span = &g_array_index (tex_2ds->slice_y_spans, CoglSpan, y);

      /* Iterate horizontal slices */
      for (x = 0; x < tex_2ds->slice_x_spans->len; ++x)
        {
          int slice_num = y * tex_2ds->slice_x_spans->len + x;

          x_span = &g_array_index (tex_2ds->slice_x_spans, CoglSpan, x);

          /* Pick the gl texture object handle */
          gl_handle = g_array_index (tex_2ds->slice_gl_handles, GLuint, slice_num);

          _cogl_texture_driver_upload_subregion_to_gl (
                                     tex_2ds->gl_target,
                                     gl_handle,
                                     tex_2ds->is_foreign,
                                     x_span->start, /* src x */
                                     y_span->start, /* src y */
                                     0, /* dst x */
                                     0, /* dst y */
                                     x_span->size - x_span->waste, /* width */
                                     y_span->size - y_span->waste, /* height */
                                     bmp,
                                     gl_format,
                                     gl_type);

          /* Keep a copy of the first pixel if needed */
          if (tex_2ds->first_pixels)
            {
              guint8 *data;

              if ((data = _cogl_bitmap_map (bmp, COGL_BUFFER_ACCESS_READ, 0)))
                {
                  memcpy (tex_2ds->first_pixels[slice_num].data,
                          data + x_span->start * bpp
                          + y_span->start * _cogl_bitmap_get_rowstride (bmp),
                          bpp);
                  _cogl_bitmap_unmap (bmp);
                }

              tex_2ds->first_pixels[slice_num].gl_format = gl_format;
              tex_2ds->first_pixels[slice_num].gl_type = gl_type;
            }

          /* Fill the waste with a copies of the rightmost pixels */
          if (x_span->waste > 0)
            {
              int bmp_rowstride = _cogl_bitmap_get_rowstride (bmp);
              guint8 *bmp_data = _cogl_bitmap_map (bmp,
                                                   COGL_BUFFER_ACCESS_READ, 0);

              if (bmp_data)
                {
                  const guint8 *src = bmp_data
                    + y_span->start * bmp_rowstride
                    + (x_span->start + x_span->size - x_span->waste - 1) * bpp;
                  guint8 *dst = waste_buf;
                  unsigned int wx, wy;

                  for (wy = 0; wy < y_span->size - y_span->waste; wy++)
                    {
                      for (wx = 0; wx < x_span->waste; wx++)
                        {
                          memcpy (dst, src, bpp);
                          dst += bpp;
                        }
                      src += bmp_rowstride;
                    }

                  _cogl_bitmap_unmap (bmp);

                  _cogl_texture_driver_prep_gl_for_pixels_upload (
                                                          x_span->waste * bpp,
                                                          bpp);

                  GE( glTexSubImage2D (tex_2ds->gl_target, 0,
                                       x_span->size - x_span->waste,
                                       0,
                                       x_span->waste,
                                       y_span->size - y_span->waste,
                                       gl_format, gl_type,
                                       waste_buf) );
                }
            }

          if (y_span->waste > 0)
            {
              int bmp_rowstride = _cogl_bitmap_get_rowstride (bmp);
              guint8 *bmp_data = _cogl_bitmap_map (bmp,
                                                   COGL_BUFFER_ACCESS_READ, 0);

              if (bmp_data)
                {
                  const guint8 *src = bmp_data
                    + ((y_span->start + y_span->size - y_span->waste - 1)
                       * bmp_rowstride)
                    + x_span->start * bpp;
                  guint8 *dst = waste_buf;
                  unsigned int wy, wx;

                  for (wy = 0; wy < y_span->waste; wy++)
                    {
                      memcpy (dst, src, (x_span->size - x_span->waste) * bpp);
                      dst += (x_span->size - x_span->waste) * bpp;

                      for (wx = 0; wx < x_span->waste; wx++)
                        {
                          memcpy (dst, dst - bpp, bpp);
                          dst += bpp;
                        }
                    }

                  _cogl_bitmap_unmap (bmp);

                  _cogl_texture_driver_prep_gl_for_pixels_upload (
                                                           x_span->size * bpp,
                                                           bpp);

                  GE( glTexSubImage2D (tex_2ds->gl_target, 0,
                                       0,
                                       y_span->size - y_span->waste,
                                       x_span->size,
                                       y_span->waste,
                                       gl_format, gl_type,
                                       waste_buf) );
                }
            }
        }
    }

  if (waste_buf)
    g_free (waste_buf);

  tex_2ds->mipmaps_dirty = TRUE;

  return TRUE;
}

static gboolean
_cogl_texture_2d_sliced_upload_subregion_to_gl (CoglTexture2DSliced *tex_2ds,
                                                int          src_x,
                                                int          src_y,
                                                int          dst_x,
                                                int          dst_y,
                                                int          width,
                                                int          height,
                                                CoglBitmap  *source_bmp,
                                                GLuint       source_gl_format,
                                                GLuint       source_gl_type)
{
  CoglSpan *x_span;
  CoglSpan *y_span;
  int               bpp;
  CoglSpanIter      x_iter;
  CoglSpanIter      y_iter;
  GLuint            gl_handle;
  int               source_x = 0, source_y = 0;
  int               inter_w = 0, inter_h = 0;
  int               local_x = 0, local_y = 0;
  guint8           *waste_buf;
  CoglPixelFormat   source_format;

  source_format = _cogl_bitmap_get_format (source_bmp);
  bpp = _cogl_get_format_bpp (source_format);

  waste_buf =
    _cogl_texture_2d_sliced_allocate_waste_buffer (tex_2ds, source_format);

  /* Iterate vertical spans */
  for (source_y = src_y,
       _cogl_span_iter_begin (&y_iter,
                              tex_2ds->slice_y_spans,
                              tex_2ds->height,
                              dst_y,
                              dst_y + height);

       !_cogl_span_iter_end (&y_iter);

       _cogl_span_iter_next (&y_iter),
       source_y += inter_h )
    {
      /* Discard slices out of the subregion early */
      if (!y_iter.intersects)
        {
          inter_h = 0;
          continue;
        }

      y_span = &g_array_index (tex_2ds->slice_y_spans, CoglSpan,
                               y_iter.index);

      /* Iterate horizontal spans */
      for (source_x = src_x,
           _cogl_span_iter_begin (&x_iter,
                                  tex_2ds->slice_x_spans,
                                  tex_2ds->width,
                                  dst_x,
                                  dst_x + width);

           !_cogl_span_iter_end (&x_iter);

           _cogl_span_iter_next (&x_iter),
           source_x += inter_w )
        {
          int slice_num;

          /* Discard slices out of the subregion early */
          if (!x_iter.intersects)
            {
              inter_w = 0;
              continue;
            }

          x_span = &g_array_index (tex_2ds->slice_x_spans, CoglSpan,
                                   x_iter.index);

          /* Pick intersection width and height */
          inter_w =  (x_iter.intersect_end - x_iter.intersect_start);
          inter_h =  (y_iter.intersect_end - y_iter.intersect_start);

          /* Localize intersection top-left corner to slice*/
          local_x =  (x_iter.intersect_start - x_iter.pos);
          local_y =  (y_iter.intersect_start - y_iter.pos);

          slice_num = y_iter.index * tex_2ds->slice_x_spans->len + x_iter.index;

          /* Pick slice GL handle */
          gl_handle = g_array_index (tex_2ds->slice_gl_handles, GLuint, slice_num);

          _cogl_texture_driver_upload_subregion_to_gl (tex_2ds->gl_target,
                                                       gl_handle,
                                                       tex_2ds->is_foreign,
                                                       source_x,
                                                       source_y,
                                                       local_x, /* dst x */
                                                       local_y, /* dst x */
                                                       inter_w, /* width */
                                                       inter_h, /* height */
                                                       source_bmp,
                                                       source_gl_format,
                                                       source_gl_type);

          /* Keep a copy of the first pixel if needed */
          if (tex_2ds->first_pixels && local_x == 0 && local_y == 0)
            {
              guint8 *data;

              if ((data = _cogl_bitmap_map (source_bmp,
                                            COGL_BUFFER_ACCESS_READ, 0)))
                {
                  memcpy (tex_2ds->first_pixels[slice_num].data,
                          data + source_x * bpp
                          + source_y * _cogl_bitmap_get_rowstride (source_bmp),
                          bpp);
                  _cogl_bitmap_unmap (source_bmp);
                }

              tex_2ds->first_pixels[slice_num].gl_format = source_gl_format;
              tex_2ds->first_pixels[slice_num].gl_type = source_gl_type;
            }

          /* If the x_span is sliced and the upload touches the
             rightmost pixels then fill the waste with copies of the
             pixels */
          if (x_span->waste > 0
              && local_x < x_span->size - x_span->waste
              && local_x + inter_w >= x_span->size - x_span->waste)
            {
              int bmp_rowstride = _cogl_bitmap_get_rowstride (source_bmp);
              guint8 *bmp_data = _cogl_bitmap_map (source_bmp,
                                                   COGL_BUFFER_ACCESS_READ, 0);

              if (bmp_data)
                {
                  const guint8 *src;
                  guint8 *dst;
                  unsigned int wx, wy;

                  src = bmp_data
                    + (src_y +  ((int)y_iter.intersect_start)
                       - dst_y)
                    * bmp_rowstride
                    + (src_x + x_span->start + x_span->size - x_span->waste
                       - dst_x - 1)
                    * bpp;

                  dst = waste_buf;

                  for (wy = 0; wy < inter_h; wy++)
                    {
                      for (wx = 0; wx < x_span->waste; wx++)
                        {
                          memcpy (dst, src, bpp);
                          dst += bpp;
                        }
                      src += bmp_rowstride;
                    }

                  _cogl_bitmap_unmap (source_bmp);

                  _cogl_texture_driver_prep_gl_for_pixels_upload (
                                                           x_span->waste * bpp,
                                                           bpp);

                  GE( glTexSubImage2D (tex_2ds->gl_target, 0,
                                       x_span->size - x_span->waste,
                                       local_y,
                                       x_span->waste,
                                       inter_h,
                                       source_gl_format,
                                       source_gl_type,
                                       waste_buf) );
                }
            }

          /* same for the bottom-most pixels */
          if (y_span->waste > 0
              && local_y < y_span->size - y_span->waste
              && local_y + inter_h >= y_span->size - y_span->waste)
            {
              int bmp_rowstride = _cogl_bitmap_get_rowstride (source_bmp);
              guint8 *bmp_data = _cogl_bitmap_map (source_bmp,
                                                   COGL_BUFFER_ACCESS_READ, 0);

              if (bmp_data)
                {
                  const guint8 *src;
                  guint8 *dst;
                  unsigned int wy, wx;
                  unsigned int copy_width;

                  src = bmp_data
                    + (src_x +  ((int)x_iter.intersect_start)
                       - dst_x)
                    * bpp
                    + (src_y + y_span->start + y_span->size - y_span->waste
                       - dst_y - 1)
                    * bmp_rowstride;

                  dst = waste_buf;

                  if (local_x + inter_w >= x_span->size - x_span->waste)
                    copy_width = x_span->size - local_x;
                  else
                    copy_width = inter_w;

                  for (wy = 0; wy < y_span->waste; wy++)
                    {
                      memcpy (dst, src, inter_w * bpp);
                      dst += inter_w * bpp;

                      for (wx = inter_w; wx < copy_width; wx++)
                        {
                          memcpy (dst, dst - bpp, bpp);
                          dst += bpp;
                        }
                    }

                  _cogl_bitmap_unmap (source_bmp);

                  _cogl_texture_driver_prep_gl_for_pixels_upload (
                                                              copy_width * bpp,
                                                              bpp);

                  GE( glTexSubImage2D (tex_2ds->gl_target, 0,
                                       local_x,
                                       y_span->size - y_span->waste,
                                       copy_width,
                                       y_span->waste,
                                       source_gl_format,
                                       source_gl_type,
                                       waste_buf) );
                }
            }
        }
    }

  if (waste_buf)
    g_free (waste_buf);

  tex_2ds->mipmaps_dirty = TRUE;

  return TRUE;
}

static int
_cogl_rect_slices_for_size (int     size_to_fill,
                            int     max_span_size,
                            int     max_waste,
                            GArray *out_spans)
{
  int       n_spans = 0;
  CoglSpan  span;

  /* Init first slice span */
  span.start = 0;
  span.size = max_span_size;
  span.waste = 0;

  /* Repeat until whole area covered */
  while (size_to_fill >= span.size)
    {
      /* Add another slice span of same size */
      if (out_spans)
        g_array_append_val (out_spans, span);
      span.start   += span.size;
      size_to_fill -= span.size;
      n_spans++;
    }

  /* Add one last smaller slice span */
  if (size_to_fill > 0)
    {
      span.size = size_to_fill;
      if (out_spans)
        g_array_append_val (out_spans, span);
      n_spans++;
    }

  return n_spans;
}

static int
_cogl_pot_slices_for_size (int    size_to_fill,
                           int    max_span_size,
                           int    max_waste,
                           GArray *out_spans)
{
  int      n_spans = 0;
  CoglSpan span;

  /* Init first slice span */
  span.start = 0;
  span.size = max_span_size;
  span.waste = 0;

  /* Fix invalid max_waste */
  if (max_waste < 0)
    max_waste = 0;

  while (TRUE)
    {
      /* Is the whole area covered? */
      if (size_to_fill > span.size)
        {
          /* Not yet - add a span of this size */
          if (out_spans)
            g_array_append_val (out_spans, span);

          span.start   += span.size;
          size_to_fill -= span.size;
          n_spans++;
        }
      else if (span.size - size_to_fill <= max_waste)
        {
          /* Yes and waste is small enough */
          span.waste = span.size - size_to_fill;
          if (out_spans)
            g_array_append_val (out_spans, span);

          return ++n_spans;
        }
      else
        {
          /* Yes but waste is too large */
          while (span.size - size_to_fill > max_waste)
            {
              span.size /= 2;
              g_assert (span.size > 0);
            }
        }
    }

  /* Can't get here */
  return 0;
}

static void
_cogl_texture_2d_sliced_set_wrap_mode_parameters (CoglTexture *tex,
                                                  GLenum wrap_mode_s,
                                                  GLenum wrap_mode_t,
                                                  GLenum wrap_mode_p)
{
  CoglTexture2DSliced *tex_2ds = COGL_TEXTURE_2D_SLICED (tex);

  /* Only set the wrap mode if it's different from the current value
     to avoid too many GL calls. Texture 2D doesn't make use of the r
     coordinate so we can ignore its wrap mode */
  if (tex_2ds->wrap_mode_s != wrap_mode_s ||
      tex_2ds->wrap_mode_t != wrap_mode_t)
    {
      int i;

      for (i = 0; i < tex_2ds->slice_gl_handles->len; i++)
        {
          GLuint texnum = g_array_index (tex_2ds->slice_gl_handles, GLuint, i);

          _cogl_bind_gl_texture_transient (tex_2ds->gl_target,
                                           texnum,
                                           tex_2ds->is_foreign);
          GE( glTexParameteri (tex_2ds->gl_target,
                               GL_TEXTURE_WRAP_S, wrap_mode_s) );
          GE( glTexParameteri (tex_2ds->gl_target,
                               GL_TEXTURE_WRAP_T, wrap_mode_t) );
        }

      tex_2ds->wrap_mode_s = wrap_mode_s;
      tex_2ds->wrap_mode_t = wrap_mode_t;
    }
}

static gboolean
_cogl_texture_2d_sliced_slices_create (CoglTexture2DSliced *tex_2ds,
                                       int width, int height,
                                       GLenum gl_intformat,
                                       GLenum gl_format,
                                       GLenum gl_type)
{
  int       max_width;
  int       max_height;
  GLuint   *gl_handles;
  int       n_x_slices;
  int       n_y_slices;
  int       n_slices;
  int       x, y;
  CoglSpan *x_span;
  CoglSpan *y_span;
  const GLfloat transparent_color[4] = { 0x00, 0x00, 0x00, 0x00 };

  int   (*slices_for_size) (int, int, int, GArray*);

  /* Initialize size of largest slice according to supported features */
  if (cogl_features_available (COGL_FEATURE_TEXTURE_NPOT))
    {
      max_width = width;
      max_height = height;
      tex_2ds->gl_target  = GL_TEXTURE_2D;
      slices_for_size = _cogl_rect_slices_for_size;
    }
  else
    {
      max_width = _cogl_util_next_p2 (width);
      max_height = _cogl_util_next_p2 (height);
      tex_2ds->gl_target = GL_TEXTURE_2D;
      slices_for_size = _cogl_pot_slices_for_size;
    }

  /* Negative number means no slicing forced by the user */
  if (tex_2ds->max_waste <= -1)
    {
      CoglSpan span;

      /* Check if size supported else bail out */
      if (!_cogl_texture_driver_size_supported (tex_2ds->gl_target,
                                                gl_intformat,
                                                gl_type,
                                                max_width,
                                                max_height))
        {
          return FALSE;
        }

      n_x_slices = 1;
      n_y_slices = 1;

      /* Init span arrays */
      tex_2ds->slice_x_spans = g_array_sized_new (FALSE, FALSE,
                                                  sizeof (CoglSpan),
                                                  1);

      tex_2ds->slice_y_spans = g_array_sized_new (FALSE, FALSE,
                                                  sizeof (CoglSpan),
                                                  1);

      /* Add a single span for width and height */
      span.start = 0;
      span.size = max_width;
      span.waste = max_width - width;
      g_array_append_val (tex_2ds->slice_x_spans, span);

      span.size = max_height;
      span.waste = max_height - height;
      g_array_append_val (tex_2ds->slice_y_spans, span);
    }
  else
    {
      /* Decrease the size of largest slice until supported by GL */
      while (!_cogl_texture_driver_size_supported (tex_2ds->gl_target,
                                                   gl_intformat,
                                                   gl_type,
                                                   max_width,
                                                   max_height))
        {
          /* Alternate between width and height */
          if (max_width > max_height)
            max_width /= 2;
          else
            max_height /= 2;

          if (max_width == 0 || max_height == 0)
            return FALSE;
        }

      /* Determine the slices required to cover the bitmap area */
      n_x_slices = slices_for_size (width,
                                    max_width, tex_2ds->max_waste,
                                    NULL);

      n_y_slices = slices_for_size (height,
                                    max_height, tex_2ds->max_waste,
                                    NULL);

      /* Init span arrays with reserved size */
      tex_2ds->slice_x_spans = g_array_sized_new (FALSE, FALSE,
                                                  sizeof (CoglSpan),
                                                  n_x_slices);

      tex_2ds->slice_y_spans = g_array_sized_new (FALSE, FALSE,
                                                  sizeof (CoglSpan),
                                                  n_y_slices);

      /* Fill span arrays with info */
      slices_for_size (width,
                       max_width, tex_2ds->max_waste,
                       tex_2ds->slice_x_spans);

      slices_for_size (height,
                       max_height, tex_2ds->max_waste,
                       tex_2ds->slice_y_spans);
    }

  /* Init and resize GL handle array */
  n_slices = n_x_slices * n_y_slices;

  tex_2ds->slice_gl_handles = g_array_sized_new (FALSE, FALSE,
                                                 sizeof (GLuint),
                                                 n_slices);

  g_array_set_size (tex_2ds->slice_gl_handles, n_slices);

  /* Allocate some space to store a copy of the first pixel of each
     slice. This is only needed if glGenerateMipmap (which is part of
     the FBO extension) is not available */
  if (cogl_features_available (COGL_FEATURE_OFFSCREEN))
    tex_2ds->first_pixels = NULL;
  else
    tex_2ds->first_pixels = g_new (CoglTexturePixel, n_slices);

  /* Wrap mode not yet set */
  tex_2ds->wrap_mode_s = GL_FALSE;
  tex_2ds->wrap_mode_t = GL_FALSE;

  /* Generate a "working set" of GL texture objects
   * (some implementations might supported faster
   *  re-binding between textures inside a set) */
  gl_handles = (GLuint*) tex_2ds->slice_gl_handles->data;

  _cogl_texture_driver_gen (GL_TEXTURE_2D, n_slices, gl_handles);

  /* Init each GL texture object */
  for (y = 0; y < n_y_slices; ++y)
    {
      y_span = &g_array_index (tex_2ds->slice_y_spans, CoglSpan, y);

      for (x = 0; x < n_x_slices; ++x)
        {
          x_span = &g_array_index (tex_2ds->slice_x_spans, CoglSpan, x);

          COGL_NOTE (SLICING, "CREATE SLICE (%d,%d)\tsize (%d,%d)",
                     x, y,
                     x_span->size - x_span->waste,
                     y_span->size - y_span->waste);

          /* Setup texture parameters */
          _cogl_bind_gl_texture_transient (tex_2ds->gl_target,
                                           gl_handles[y * n_x_slices + x],
                                           FALSE);

          _cogl_texture_driver_try_setting_gl_border_color (tex_2ds->gl_target,
                                                            transparent_color);

          /* Pass NULL data to init size and internal format */
          GE( glTexImage2D (tex_2ds->gl_target, 0, gl_intformat,
                            x_span->size, y_span->size, 0,
                            gl_format, gl_type, NULL) );
        }
    }

  return TRUE;
}

static void
_cogl_texture_2d_sliced_slices_free (CoglTexture2DSliced *tex_2ds)
{
  if (tex_2ds->slice_x_spans != NULL)
    g_array_free (tex_2ds->slice_x_spans, TRUE);

  if (tex_2ds->slice_y_spans != NULL)
    g_array_free (tex_2ds->slice_y_spans, TRUE);

  if (tex_2ds->slice_gl_handles != NULL)
    {
      int i;
      if (tex_2ds->is_foreign == FALSE)
        for (i = 0; i < tex_2ds->slice_gl_handles->len; i++)
          {
            GLuint texture =
              g_array_index (tex_2ds->slice_gl_handles, GLuint, i);
            _cogl_delete_gl_texture (texture);
          }

      g_array_free (tex_2ds->slice_gl_handles, TRUE);
    }

  if (tex_2ds->first_pixels != NULL)
    g_free (tex_2ds->first_pixels);
}

static void
_cogl_texture_2d_sliced_free (CoglTexture2DSliced *tex_2ds)
{
  _cogl_texture_2d_sliced_slices_free (tex_2ds);

  /* Chain up */
  _cogl_texture_free (COGL_TEXTURE (tex_2ds));
}

static gboolean
_cogl_texture_2d_sliced_init_base (CoglTexture2DSliced *tex_2ds,
                                   int width,
                                   int height,
                                   GLenum gl_intformat,
                                   GLenum gl_format,
                                   GLenum gl_type,
                                   CoglPixelFormat internal_format)
{
  CoglTexture *tex = COGL_TEXTURE (tex_2ds);

  tex->vtable = &cogl_texture_2d_sliced_vtable;

  tex_2ds->is_foreign = FALSE;
  tex_2ds->auto_mipmap = FALSE;
  tex_2ds->mipmaps_dirty = TRUE;
  tex_2ds->first_pixels = NULL;

  tex_2ds->slice_x_spans = NULL;
  tex_2ds->slice_y_spans = NULL;
  tex_2ds->slice_gl_handles = NULL;

  /* We default to GL_LINEAR for both filters */
  tex_2ds->min_filter = GL_LINEAR;
  tex_2ds->mag_filter = GL_LINEAR;

  /* Create slices for the given format and size */
  if (!_cogl_texture_2d_sliced_slices_create (tex_2ds,
                                              width,
                                              height,
                                              gl_intformat,
                                              gl_format,
                                              gl_type))
    return FALSE;

  tex_2ds->gl_format = gl_intformat;
  tex_2ds->width = width;
  tex_2ds->height = height;
  tex_2ds->format = internal_format;

  return TRUE;
}

CoglHandle
_cogl_texture_2d_sliced_new_with_size (unsigned int     width,
                                       unsigned int     height,
                                       CoglTextureFlags flags,
                                       CoglPixelFormat  internal_format)
{
  CoglTexture2DSliced   *tex_2ds;
  GLenum                 gl_intformat;
  GLenum                 gl_format;
  GLenum                 gl_type;

  /* Since no data, we need some internal format */
  if (internal_format == COGL_PIXEL_FORMAT_ANY)
    internal_format = COGL_PIXEL_FORMAT_RGBA_8888_PRE;

  /* Init texture with empty bitmap */
  tex_2ds = g_new (CoglTexture2DSliced, 1);

  if ((flags & COGL_TEXTURE_NO_SLICING))
    tex_2ds->max_waste = -1;
  else
    tex_2ds->max_waste = COGL_TEXTURE_MAX_WASTE;

  /* Find closest GL format match */
  internal_format = _cogl_pixel_format_to_gl (internal_format,
                                              &gl_intformat,
                                              &gl_format,
                                              &gl_type);

  if (!_cogl_texture_2d_sliced_init_base (tex_2ds,
                                          width, height,
                                          gl_intformat,
                                          gl_format,
                                          gl_type,
                                          internal_format))
    {
      _cogl_texture_2d_sliced_free (tex_2ds);
      return COGL_INVALID_HANDLE;
    }

  tex_2ds->auto_mipmap = (flags & COGL_TEXTURE_NO_AUTO_MIPMAP) == 0;

  return _cogl_texture_2d_sliced_handle_new (tex_2ds);
}

CoglHandle
_cogl_texture_2d_sliced_new_from_bitmap (CoglBitmap      *bmp,
                                         CoglTextureFlags flags,
                                         CoglPixelFormat  internal_format)
{
  CoglTexture2DSliced *tex_2ds;
  CoglBitmap          *dst_bmp;
  GLenum               gl_intformat;
  GLenum               gl_format;
  GLenum               gl_type;
  int                  width, height;

  g_return_val_if_fail (cogl_is_bitmap (bmp), COGL_INVALID_HANDLE);

  width = _cogl_bitmap_get_width (bmp);
  height = _cogl_bitmap_get_height (bmp);

  /* Create new texture and fill with loaded data */
  tex_2ds = g_new0 (CoglTexture2DSliced, 1);

  if (flags & COGL_TEXTURE_NO_SLICING)
    tex_2ds->max_waste = -1;
  else
    tex_2ds->max_waste = COGL_TEXTURE_MAX_WASTE;

  /* FIXME: If upload fails we should set some kind of
   * error flag but still return texture handle if the
   * user decides to destroy another texture and upload
   * this one instead (reloading from file is not needed
   * in that case). As a rule then, everytime a valid
   * CoglHandle is returned, it should also be destroyed
   * with cogl_handle_unref at some point! */

  dst_bmp = _cogl_texture_prepare_for_upload (bmp,
                                              internal_format,
                                              &internal_format,
                                              &gl_intformat,
                                              &gl_format,
                                              &gl_type);
  if (dst_bmp == COGL_INVALID_HANDLE)
    {
      _cogl_texture_2d_sliced_free (tex_2ds);
      return COGL_INVALID_HANDLE;
    }

  if (!_cogl_texture_2d_sliced_init_base (tex_2ds,
                                          width, height,
                                          gl_intformat,
                                          gl_format,
                                          gl_type,
                                          internal_format))
    goto error;

  if (!_cogl_texture_2d_sliced_upload_to_gl (tex_2ds,
                                             dst_bmp,
                                             gl_intformat,
                                             gl_format,
                                             gl_type))
    goto error;

  cogl_object_unref (dst_bmp);

  tex_2ds->auto_mipmap = (flags & COGL_TEXTURE_NO_AUTO_MIPMAP) == 0;

  return _cogl_texture_2d_sliced_handle_new (tex_2ds);

 error:
  cogl_object_unref (dst_bmp);
  _cogl_texture_2d_sliced_free (tex_2ds);
  return COGL_INVALID_HANDLE;
}

CoglHandle
_cogl_texture_2d_sliced_new_from_foreign (GLuint           gl_handle,
                                          GLenum           gl_target,
                                          GLuint           width,
                                          GLuint           height,
                                          GLuint           x_pot_waste,
                                          GLuint           y_pot_waste,
                                          CoglPixelFormat  format)
{
  /* NOTE: width, height and internal format are not queriable
   * in GLES, hence such a function prototype.
   */

  GLenum               gl_error = 0;
  GLboolean            gl_istexture;
  GLint                gl_compressed = GL_FALSE;
  GLint                gl_int_format = 0;
  GLint                gl_width = 0;
  GLint                gl_height = 0;
  GLint                gl_gen_mipmap;
  CoglTexture2DSliced *tex_2ds;
  CoglTexture         *tex;
  CoglSpan             x_span;
  CoglSpan             y_span;

  if (!_cogl_texture_driver_allows_foreign_gl_target (gl_target))
    return COGL_INVALID_HANDLE;

#if HAVE_COGL_GL
  /* It shouldn't be necissary to have waste in this case since
   * the texture isn't limited to power of two sizes. */
  if (gl_target == GL_TEXTURE_RECTANGLE_ARB &&
      (x_pot_waste != 0 || y_pot_waste != 0))
    {
      g_warning ("You can't create a foreign GL_TEXTURE_RECTANGLE cogl "
                 "texture with waste\n");
      return COGL_INVALID_HANDLE;
    }
#endif

  /* Make sure it is a valid GL texture object */
  gl_istexture = glIsTexture (gl_handle);
  if (gl_istexture == GL_FALSE)
    return COGL_INVALID_HANDLE;

  /* Make sure binding succeeds */
  while ((gl_error = glGetError ()) != GL_NO_ERROR)
    ;

  _cogl_bind_gl_texture_transient (gl_target, gl_handle, TRUE);
  if (glGetError () != GL_NO_ERROR)
    return COGL_INVALID_HANDLE;

  /* Obtain texture parameters
     (only level 0 we are interested in) */

#if HAVE_COGL_GL
  GE( glGetTexLevelParameteriv (gl_target, 0,
                                GL_TEXTURE_COMPRESSED,
                                &gl_compressed) );

  GE( glGetTexLevelParameteriv (gl_target, 0,
                                GL_TEXTURE_INTERNAL_FORMAT,
                                &gl_int_format) );
#endif

  /* Note: We always trust the given width and height without querying
   * the texture object because the user may be creating a Cogl
   * texture for a texture_from_pixmap object where glTexImage2D may
   * not have been called and the texture_from_pixmap spec doesn't
   * clarify that it is reliable to query back the size from OpenGL.
   */
  gl_width = width + x_pot_waste;
  gl_height = height + y_pot_waste;

  GE( glGetTexParameteriv (gl_target,
                           GL_GENERATE_MIPMAP,
                           &gl_gen_mipmap) );

  /* Validate width and height */
  if (gl_width <= 0 || gl_height <= 0)
    return COGL_INVALID_HANDLE;

  /* Validate pot waste */
  if (x_pot_waste < 0 || x_pot_waste >= gl_width ||
      y_pot_waste < 0 || y_pot_waste >= gl_height)
    return COGL_INVALID_HANDLE;

  /* Compressed texture images not supported */
  if (gl_compressed == GL_TRUE)
    return COGL_INVALID_HANDLE;

  /* Try and match to a cogl format */
  if (!_cogl_pixel_format_from_gl_internal (gl_int_format, &format))
    return COGL_INVALID_HANDLE;

  /* Create new texture */
  tex_2ds = g_new0 (CoglTexture2DSliced, 1);

  tex = COGL_TEXTURE (tex_2ds);
  tex->vtable = &cogl_texture_2d_sliced_vtable;

  /* Setup bitmap info */
  tex_2ds->is_foreign = TRUE;
  tex_2ds->auto_mipmap = (gl_gen_mipmap == GL_TRUE) ? TRUE : FALSE;
  tex_2ds->mipmaps_dirty = TRUE;
  tex_2ds->first_pixels = NULL;

  tex_2ds->format = format;
  tex_2ds->width = gl_width - x_pot_waste;
  tex_2ds->height = gl_height - y_pot_waste;
  tex_2ds->gl_target = gl_target;
  tex_2ds->gl_format = gl_int_format;

  /* Unknown filter */
  tex_2ds->min_filter = GL_FALSE;
  tex_2ds->mag_filter = GL_FALSE;
  tex_2ds->max_waste = 0;

  /* Wrap mode not yet set */
  tex_2ds->wrap_mode_s = GL_FALSE;
  tex_2ds->wrap_mode_t = GL_FALSE;

  /* Create slice arrays */
  tex_2ds->slice_x_spans =
    g_array_sized_new (FALSE, FALSE,
                       sizeof (CoglSpan), 1);

  tex_2ds->slice_y_spans =
    g_array_sized_new (FALSE, FALSE,
                       sizeof (CoglSpan), 1);

  tex_2ds->slice_gl_handles =
    g_array_sized_new (FALSE, FALSE,
                       sizeof (GLuint), 1);

  /* Store info for a single slice */
  x_span.start = 0;
  x_span.size = gl_width;
  x_span.waste = x_pot_waste;
  g_array_append_val (tex_2ds->slice_x_spans, x_span);

  y_span.start = 0;
  y_span.size = gl_height;
  y_span.waste = y_pot_waste;
  g_array_append_val (tex_2ds->slice_y_spans, y_span);

  g_array_append_val (tex_2ds->slice_gl_handles, gl_handle);

  tex_2ds->first_pixels = NULL;

  return _cogl_texture_2d_sliced_handle_new (tex_2ds);
}

static gboolean
_cogl_texture_2d_sliced_is_foreign (CoglTexture *tex)
{
  return COGL_TEXTURE_2D_SLICED (tex)->is_foreign;
}

static int
_cogl_texture_2d_sliced_get_max_waste (CoglTexture *tex)
{
  CoglTexture2DSliced *tex_2ds = COGL_TEXTURE_2D_SLICED (tex);

  return tex_2ds->max_waste;
}

static gboolean
_cogl_texture_2d_sliced_is_sliced (CoglTexture *tex)
{
  CoglTexture2DSliced *tex_2ds = COGL_TEXTURE_2D_SLICED (tex);

  if (tex_2ds->slice_gl_handles == NULL)
    return FALSE;

  if (tex_2ds->slice_gl_handles->len <= 1)
    return FALSE;

  return TRUE;
}

static gboolean
_cogl_texture_2d_sliced_can_hardware_repeat (CoglTexture *tex)
{
  CoglTexture2DSliced *tex_2ds = COGL_TEXTURE_2D_SLICED (tex);
  CoglSpan *x_span;
  CoglSpan *y_span;

  x_span = &g_array_index (tex_2ds->slice_x_spans, CoglSpan, 0);
  y_span = &g_array_index (tex_2ds->slice_y_spans, CoglSpan, 0);

#if HAVE_COGL_GL
  /* TODO: COGL_TEXTURE_TYPE_2D_RECTANGLE */
  if (tex_2ds->gl_target == GL_TEXTURE_RECTANGLE_ARB)
    return FALSE;
#endif

  return (x_span->waste || y_span->waste) ? FALSE : TRUE;
}

static void
_cogl_texture_2d_sliced_transform_coords_to_gl (CoglTexture *tex,
                                                float *s,
                                                float *t)
{
  CoglTexture2DSliced *tex_2ds = COGL_TEXTURE_2D_SLICED (tex);
  CoglSpan *x_span;
  CoglSpan *y_span;

  g_assert (!_cogl_texture_2d_sliced_is_sliced (tex));

  /* Don't include the waste in the texture coordinates */
  x_span = &g_array_index (tex_2ds->slice_x_spans, CoglSpan, 0);
  y_span = &g_array_index (tex_2ds->slice_y_spans, CoglSpan, 0);

  *s *= tex_2ds->width / (float)x_span->size;
  *t *= tex_2ds->height / (float)y_span->size;

#if HAVE_COGL_GL
  /* Denormalize texture coordinates for rectangle textures */
  if (tex_2ds->gl_target == GL_TEXTURE_RECTANGLE_ARB)
    {
      *s *= x_span->size;
      *t *= y_span->size;
    }
#endif
}

static CoglTransformResult
_cogl_texture_2d_sliced_transform_quad_coords_to_gl (CoglTexture *tex,
                                                     float *coords)
{
  gboolean need_repeat = FALSE;
  int i;

  /* This is a bit lazy - in the case where the quad lies entirely
   * within a single slice we could avoid the fallback. But that
   * could likely lead to visual inconsistency if the fallback involves
   * dropping layers, so this might be the right thing to do anyways.
   */
  if (_cogl_texture_2d_sliced_is_sliced (tex))
    return COGL_TRANSFORM_SOFTWARE_REPEAT;

  for (i = 0; i < 4; i++)
    if (coords[i] < 0.0f || coords[i] > 1.0f)
      need_repeat = TRUE;

  if (need_repeat && !_cogl_texture_2d_sliced_can_hardware_repeat (tex))
    return COGL_TRANSFORM_SOFTWARE_REPEAT;

  _cogl_texture_2d_sliced_transform_coords_to_gl (tex, coords + 0, coords + 1);
  _cogl_texture_2d_sliced_transform_coords_to_gl (tex, coords + 2, coords + 3);

  return (need_repeat
          ? COGL_TRANSFORM_HARDWARE_REPEAT : COGL_TRANSFORM_NO_REPEAT);
}

static gboolean
_cogl_texture_2d_sliced_get_gl_texture (CoglTexture *tex,
                                        GLuint *out_gl_handle,
                                        GLenum *out_gl_target)
{
  CoglTexture2DSliced *tex_2ds = COGL_TEXTURE_2D_SLICED (tex);

  if (tex_2ds->slice_gl_handles == NULL)
    return FALSE;

  if (tex_2ds->slice_gl_handles->len < 1)
    return FALSE;

  if (out_gl_handle != NULL)
    *out_gl_handle = g_array_index (tex_2ds->slice_gl_handles, GLuint, 0);

  if (out_gl_target != NULL)
    *out_gl_target = tex_2ds->gl_target;

  return TRUE;
}

static void
_cogl_texture_2d_sliced_set_filters (CoglTexture *tex,
                                     GLenum min_filter,
                                     GLenum mag_filter)
{
  CoglTexture2DSliced *tex_2ds = COGL_TEXTURE_2D_SLICED (tex);
  GLuint               gl_handle;
  int                  i;

  /* Make sure slices were created */
  if (tex_2ds->slice_gl_handles == NULL)
    return;

  if (min_filter == tex_2ds->min_filter
      && mag_filter == tex_2ds->mag_filter)
    return;

  /* Store new values */
  tex_2ds->min_filter = min_filter;
  tex_2ds->mag_filter = mag_filter;

  /* Apply new filters to every slice */
  for (i=0; i<tex_2ds->slice_gl_handles->len; ++i)
    {
      gl_handle = g_array_index (tex_2ds->slice_gl_handles, GLuint, i);
      _cogl_bind_gl_texture_transient (tex_2ds->gl_target,
                                       gl_handle,
                                       tex_2ds->is_foreign);
      GE( glTexParameteri (tex_2ds->gl_target, GL_TEXTURE_MAG_FILTER,
                           tex_2ds->mag_filter) );
      GE( glTexParameteri (tex_2ds->gl_target, GL_TEXTURE_MIN_FILTER,
                           tex_2ds->min_filter) );
    }
}

static void
_cogl_texture_2d_sliced_pre_paint (CoglTexture *tex,
                                   CoglTexturePrePaintFlags flags)
{
  CoglTexture2DSliced *tex_2ds = COGL_TEXTURE_2D_SLICED (tex);
  int                  i;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* We only care about the mipmap flag */
  if ((flags & COGL_TEXTURE_NEEDS_MIPMAP) == 0)
    return;

  /* Only update if the mipmaps are dirty */
  if (!tex_2ds->auto_mipmap || !tex_2ds->mipmaps_dirty)
    return;

  /* Make sure slices were created */
  if (tex_2ds->slice_gl_handles == NULL)
    return;

  /* Regenerate the mipmaps on every slice */
  for (i = 0; i < tex_2ds->slice_gl_handles->len; i++)
    {
      GLuint gl_handle = g_array_index (tex_2ds->slice_gl_handles, GLuint, i);
      _cogl_bind_gl_texture_transient (tex_2ds->gl_target,
                                       gl_handle,
                                       tex_2ds->is_foreign);

      /* glGenerateMipmap is defined in the FBO extension */
      if (cogl_features_available (COGL_FEATURE_OFFSCREEN))
        _cogl_texture_driver_gl_generate_mipmaps (tex_2ds->gl_target);
      else if (tex_2ds->first_pixels)
        {
          CoglTexturePixel *pixel = tex_2ds->first_pixels + i;
          /* Temporarily enable automatic mipmap generation and
             re-upload the first pixel to cause a regeneration */
          GE( glTexParameteri (tex_2ds->gl_target, GL_GENERATE_MIPMAP, GL_TRUE) );
          GE( glTexSubImage2D (tex_2ds->gl_target, 0, 0, 0, 1, 1,
                               pixel->gl_format, pixel->gl_type,
                               pixel->data) );
          GE( glTexParameteri (tex_2ds->gl_target, GL_GENERATE_MIPMAP, GL_FALSE) );
        }
    }

  tex_2ds->mipmaps_dirty = FALSE;
}

static void
_cogl_texture_2d_sliced_ensure_non_quad_rendering (CoglTexture *tex)
{
  /* Nothing needs to be done */
}

static gboolean
_cogl_texture_2d_sliced_set_region (CoglTexture    *tex,
                                    int             src_x,
                                    int             src_y,
                                    int             dst_x,
                                    int             dst_y,
                                    unsigned int    dst_width,
                                    unsigned int    dst_height,
                                    CoglBitmap     *bmp)
{
  CoglTexture2DSliced *tex_2ds = COGL_TEXTURE_2D_SLICED (tex);
  GLenum               gl_format;
  GLenum               gl_type;

  _cogl_pixel_format_to_gl (_cogl_bitmap_get_format (bmp),
                            NULL, /* internal format */
                            &gl_format,
                            &gl_type);

  /* Send data to GL */
  _cogl_texture_2d_sliced_upload_subregion_to_gl (tex_2ds,
                                                  src_x, src_y,
                                                  dst_x, dst_y,
                                                  dst_width, dst_height,
                                                  bmp,
                                                  gl_format,
                                                  gl_type);

  return TRUE;
}

static gboolean
_cogl_texture_2d_sliced_download_from_gl (
                                      CoglTexture2DSliced *tex_2ds,
                                      CoglBitmap          *target_bmp,
                                      GLuint               target_gl_format,
                                      GLuint               target_gl_type)
{
  CoglSpan   *x_span;
  CoglSpan   *y_span;
  GLuint      gl_handle;
  int         bpp;
  int         x, y;
  CoglBitmap *slice_bmp;
  CoglPixelFormat target_format = _cogl_bitmap_get_format (target_bmp);

  bpp = _cogl_get_format_bpp (target_format);

  /* Iterate vertical slices */
  for (y = 0; y < tex_2ds->slice_y_spans->len; ++y)
    {
      y_span = &g_array_index (tex_2ds->slice_y_spans, CoglSpan, y);

      /* Iterate horizontal slices */
      for (x = 0; x < tex_2ds->slice_x_spans->len; ++x)
	{
	  /*if (x != 0 || y != 1) continue;*/
	  x_span = &g_array_index (tex_2ds->slice_x_spans, CoglSpan, x);

	  /* Pick the gl texture object handle */
	  gl_handle = g_array_index (tex_2ds->slice_gl_handles, GLuint,
				     y * tex_2ds->slice_x_spans->len + x);

	  /* If there's any waste we need to copy manually
             (no glGetTexSubImage) */

	  if (y_span->waste != 0 || x_span->waste != 0)
	    {
              int rowstride = x_span->size * bpp;
              guint8 *data = g_malloc (rowstride * y_span->size);

	      /* Setup temp bitmap for slice subregion */
              slice_bmp = _cogl_bitmap_new_from_data (data,
                                                      target_format,
                                                      x_span->size,
                                                      y_span->size,
                                                      rowstride,
                                                      (CoglBitmapDestroyNotify)
                                                      g_free,
                                                      NULL);

	      /* Setup gl alignment to 0,0 top-left corner */
              _cogl_texture_driver_prep_gl_for_pixels_download (rowstride, bpp);

	      /* Download slice image data into temp bmp */
              _cogl_bind_gl_texture_transient (tex_2ds->gl_target,
                                               gl_handle,
                                               tex_2ds->is_foreign);

              if (!_cogl_texture_driver_gl_get_tex_image (tex_2ds->gl_target,
                                                          target_gl_format,
                                                          target_gl_type,
                                                          data))
                {
                  /* Free temp bitmap */
                  cogl_object_unref (slice_bmp);
                  return FALSE;
                }

	      /* Copy portion of slice from temp to target bmp */
	      _cogl_bitmap_copy_subregion (slice_bmp,
					   target_bmp,
					   0, 0,
					   x_span->start,
					   y_span->start,
					   x_span->size - x_span->waste,
					   y_span->size - y_span->waste);
	      /* Free temp bitmap */
              cogl_object_unref (slice_bmp);
	    }
	  else
	    {
              guint8 *data;
              GLvoid *dst;
              gboolean ret;
              int rowstride = _cogl_bitmap_get_rowstride (target_bmp);

              data = _cogl_bitmap_map (target_bmp,
                                       COGL_BUFFER_ACCESS_WRITE,
                                       0);
              if (data == NULL)
                return FALSE;

              dst = data + x_span->start * bpp + y_span->start * rowstride;

              _cogl_texture_driver_prep_gl_for_pixels_download (rowstride, bpp);

	      /* Download slice image data */
              _cogl_bind_gl_texture_transient (tex_2ds->gl_target,
                                               gl_handle,
                                               tex_2ds->is_foreign);

              ret = _cogl_texture_driver_gl_get_tex_image (tex_2ds->gl_target,
                                                           target_gl_format,
                                                           target_gl_type,
                                                           dst);

              _cogl_bitmap_unmap (target_bmp);

              if (!ret)
                return ret;
	    }
	}
    }

  return TRUE;
}

static gboolean
_cogl_texture_2d_sliced_get_data (CoglTexture     *tex,
                                  CoglPixelFormat  format,
                                  unsigned int     rowstride,
                                  guint8          *data)
{
  CoglTexture2DSliced *tex_2ds = COGL_TEXTURE_2D_SLICED (tex);
  int             bpp;
  GLenum          gl_format;
  GLenum          gl_type;
  CoglBitmap     *target_bmp;
  gboolean        ret;

  bpp = _cogl_get_format_bpp (format);

  _cogl_pixel_format_to_gl (format,
                            NULL, /* internal format */
                            &gl_format,
                            &gl_type);

  target_bmp = _cogl_bitmap_new_from_data (data,
                                           format,
                                           tex_2ds->width,
                                           tex_2ds->height,
                                           rowstride,
                                           NULL, /* destroy_fn */
                                           NULL /* destroy_fn_data */);

  /* Retrieve data from slices */
  ret = _cogl_texture_2d_sliced_download_from_gl (tex_2ds,
                                                  target_bmp,
                                                  gl_format,
                                                  gl_type);

  cogl_object_unref (target_bmp);

  return ret;
}

static CoglPixelFormat
_cogl_texture_2d_sliced_get_format (CoglTexture *tex)
{
  return COGL_TEXTURE_2D_SLICED (tex)->format;
}

static GLenum
_cogl_texture_2d_sliced_get_gl_format (CoglTexture *tex)
{
  return COGL_TEXTURE_2D_SLICED (tex)->gl_format;
}

static int
_cogl_texture_2d_sliced_get_width (CoglTexture *tex)
{
  return COGL_TEXTURE_2D_SLICED (tex)->width;
}

static int
_cogl_texture_2d_sliced_get_height (CoglTexture *tex)
{
  return COGL_TEXTURE_2D_SLICED (tex)->height;
}

static const CoglTextureVtable
cogl_texture_2d_sliced_vtable =
  {
    _cogl_texture_2d_sliced_set_region,
    _cogl_texture_2d_sliced_get_data,
    _cogl_texture_2d_sliced_foreach_sub_texture_in_region,
    _cogl_texture_2d_sliced_get_max_waste,
    _cogl_texture_2d_sliced_is_sliced,
    _cogl_texture_2d_sliced_can_hardware_repeat,
    _cogl_texture_2d_sliced_transform_coords_to_gl,
    _cogl_texture_2d_sliced_transform_quad_coords_to_gl,
    _cogl_texture_2d_sliced_get_gl_texture,
    _cogl_texture_2d_sliced_set_filters,
    _cogl_texture_2d_sliced_pre_paint,
    _cogl_texture_2d_sliced_ensure_non_quad_rendering,
    _cogl_texture_2d_sliced_set_wrap_mode_parameters,
    _cogl_texture_2d_sliced_get_format,
    _cogl_texture_2d_sliced_get_gl_format,
    _cogl_texture_2d_sliced_get_width,
    _cogl_texture_2d_sliced_get_height,
    _cogl_texture_2d_sliced_is_foreign
  };
