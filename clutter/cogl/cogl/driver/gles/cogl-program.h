/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2008,2009 Intel Corporation.
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

#ifndef __COGL_PROGRAM_H
#define __COGL_PROGRAM_H

#include "cogl-gles2-wrapper.h"
#include "cogl-handle.h"

#define COGL_PROGRAM_NUM_CUSTOM_UNIFORMS 16
#define COGL_PROGRAM_UNBOUND_CUSTOM_UNIFORM -2

typedef struct _CoglProgram CoglProgram;

struct _CoglProgram
{
  CoglHandleObject   _parent;

#ifdef HAVE_COGL_GLES2
  GSList            *attached_shaders;

  char              *custom_uniform_names[COGL_PROGRAM_NUM_CUSTOM_UNIFORMS];

  CoglBoxedValue     custom_uniforms[COGL_PROGRAM_NUM_CUSTOM_UNIFORMS];

  /* Uniforms that have changed since the last time this program was
   * used. */
  guint32            dirty_custom_uniforms;
#endif
};

CoglProgram *_cogl_program_pointer_from_handle (CoglHandle handle);

#endif /* __COGL_PROGRAM_H */
