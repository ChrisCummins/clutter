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
 */

#ifndef __COGL_CONTEXT_H
#define __COGL_CONTEXT_H

#include "cogl-internal.h"
#include "cogl-context-driver.h"
#include "cogl-context-winsys.h"
#include "cogl-primitives.h"
#include "cogl-clip-stack.h"
#include "cogl-matrix-stack.h"
#include "cogl-material-private.h"
#include "cogl-buffer-private.h"
#include "cogl-bitmask.h"
#include "cogl-atlas.h"

typedef struct
{
  GLfloat v[3];
  GLfloat t[2];
  GLubyte c[4];
} CoglTextureGLVertex;

typedef struct
{
  /* Features cache */
  CoglFeatureFlags        feature_flags;
  CoglFeatureFlagsPrivate feature_flags_private;
  gboolean                features_cached;

  CoglHandle        default_material;
  CoglHandle        default_layer_0;
  CoglHandle        default_layer_n;
  CoglHandle        dummy_layer_dependant;

  /* Enable cache */
  unsigned long     enable_flags;

  gboolean          enable_backface_culling;
  CoglFrontWinding  flushed_front_winding;

  gboolean          indirect;

  /* A few handy matrix constants */
  CoglMatrix        identity_matrix;
  CoglMatrix        y_flip_matrix;

  /* Client-side matrix stack or NULL if none */
  CoglMatrixMode    flushed_matrix_mode;

  GArray           *texture_units;
  int               active_texture_unit;

  CoglMaterialFogState legacy_fog_state;

  /* Materials */
  CoglMaterial     *simple_material; /* used for set_source_color */
  CoglMaterial     *texture_material; /* used for set_source_texture */
  CoglMaterial     *source_material;
  GString          *arbfp_source_buffer;

  int               legacy_state_set;

  /* Textures */
  CoglHandle        default_gl_texture_2d_tex;
  CoglHandle        default_gl_texture_rect_tex;

  /* Batching geometry... */
  /* We journal the texture rectangles we want to submit to OpenGL so
   * we have an oppertunity to optimise the final order so that we
   * can batch things together. */
  GArray           *journal;
  GArray           *logged_vertices;
  GArray           *polygon_vertices;

  /* Some simple caching, to minimize state changes... */
  CoglMaterial     *current_material;
  unsigned long     current_material_changes_since_flush;
  gboolean          current_material_skip_gl_color;

  GArray           *material0_nodes;
  GArray           *material1_nodes;

  /* Bitmask of texture coordinates arrays that are enabled */
  CoglBitmask       texcoord_arrays_enabled;
  /* These are temporary bitmasks that are used when disabling
     texcoord arrays. They are here just to avoid allocating new ones
     each time */
  CoglBitmask       texcoord_arrays_to_disable;
  CoglBitmask       temp_bitmask;

  gboolean          gl_blend_enable_cache;

  gboolean              depth_test_enabled_cache;
  CoglDepthTestFunction depth_test_function_cache;
  gboolean              depth_writing_enabled_cache;
  float                 depth_range_near_cache;
  float                 depth_range_far_cache;

  gboolean              legacy_depth_test_enabled;

  float             point_size_cache;

  CoglBuffer       *current_buffer[COGL_BUFFER_BIND_TARGET_COUNT];

  /* Framebuffers */
  GSList           *framebuffer_stack;
  CoglHandle        window_buffer;
  gboolean          dirty_bound_framebuffer;
  gboolean          dirty_gl_viewport;

  /* Primitives */
  CoglHandle        current_path;
  CoglMaterial     *stencil_material;

  /* Pre-generated VBOs containing indices to generate GL_TRIANGLES
     out of a vertex array of quads */
  CoglHandle        quad_indices_byte;
  unsigned int      quad_indices_short_len;
  CoglHandle        quad_indices_short;

  gboolean          in_begin_gl_block;

  CoglMaterial     *texture_download_material;

  CoglAtlas        *atlas;

  /* This debugging variable is used to pick a colour for visually
     displaying the quad batches. It needs to be global so that it can
     be reset by cogl_clear. It needs to be reset to increase the
     chances of getting the same colour during an animation */
  guint8            journal_rectangles_color;

  /* Cached values for GL_MAX_TEXTURE_[IMAGE_]UNITS to avoid calling
     glGetInteger too often */
  GLint             max_texture_units;
  GLint             max_texture_image_units;
  GLint             max_activateable_texture_units;

  /* Fragment processing programs */
  CoglHandle              current_program;

  CoglMaterialProgramType current_use_program_type;
  GLuint                  current_gl_program;

  /* List of types that will be considered a subclass of CoglTexture in
     cogl_is_texture */
  GSList           *texture_types;

  /* List of types that will be considered a subclass of CoglBuffer in
     cogl_is_buffer */
  GSList           *buffer_types;

  CoglContextDriver drv;
  CoglContextWinsys winsys;
} CoglContext;

CoglContext *
_cogl_context_get_default ();

/* Obtains the context and returns retval if NULL */
#define _COGL_GET_CONTEXT(ctxvar, retval) \
CoglContext *ctxvar = _cogl_context_get_default (); \
if (ctxvar == NULL) return retval;

#define NO_RETVAL

#endif /* __COGL_CONTEXT_H */
