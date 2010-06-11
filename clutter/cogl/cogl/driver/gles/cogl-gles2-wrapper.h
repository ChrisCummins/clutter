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

#ifndef __COGL_GLES2_WRAPPER_H__
#define __COGL_GLES2_WRAPPER_H__

#include "cogl.h" /* needed for gl header include */
#include "cogl-internal.h"

G_BEGIN_DECLS

#ifdef HAVE_COGL_GLES2

typedef struct _CoglGles2Wrapper	  CoglGles2Wrapper;
typedef struct _CoglGles2WrapperTextureUnit
					  CoglGles2WrapperTextureUnit;

typedef struct _CoglGles2WrapperAttributes  CoglGles2WrapperAttributes;
typedef struct _CoglGles2WrapperUniforms  CoglGles2WrapperUniforms;
typedef struct _CoglGles2WrapperTexEnv    CoglGles2WrapperTexEnv;
typedef struct _CoglGles2WrapperTextureUnitSettings
					  CoglGles2WrapperTextureUnitSettings;
typedef struct _CoglGles2WrapperSettings  CoglGles2WrapperSettings;
typedef struct _CoglGles2WrapperProgram	  CoglGles2WrapperProgram;
typedef struct _CoglGles2WrapperShader	  CoglGles2WrapperShader;

#define COGL_GLES2_NUM_CUSTOM_UNIFORMS    16
#define COGL_GLES2_UNBOUND_CUSTOM_UNIFORM -2

/* Must be a power of two */
#define COGL_GLES2_MODELVIEW_STACK_SIZE   32
#define COGL_GLES2_PROJECTION_STACK_SIZE  2
#define COGL_GLES2_TEXTURE_STACK_SIZE     2

/* Accessors for the texture unit bit mask */
#define COGL_GLES2_TEXTURE_UNIT_IS_ENABLED(mask, unit)  \
  (((mask) & (1 << ((unit) * 2))) ? TRUE : FALSE)
#define COGL_GLES2_SET_BIT(mask, bit, val)                              \
  ((val) ? ((mask) |= (1 << (bit))) : ((mask) &= ~(1 << (bit))))
#define COGL_GLES2_TEXTURE_UNIT_SET_ENABLED(mask, unit, val)    \
  COGL_GLES2_SET_BIT ((mask), (unit) * 2, (val))

#define COGL_GLES2_MAX_TEXTURE_UNITS (sizeof (guint32) * 8 / 2)

/* Dirty flags for shader uniforms */
enum
  {
    COGL_GLES2_DIRTY_MVP_MATRIX       = 1 << 0,
    COGL_GLES2_DIRTY_MODELVIEW_MATRIX = 1 << 1,
    COGL_GLES2_DIRTY_TEXTURE_MATRICES = 1 << 2,
    COGL_GLES2_DIRTY_FOG_DENSITY      = 1 << 3,
    COGL_GLES2_DIRTY_FOG_START        = 1 << 4,
    COGL_GLES2_DIRTY_FOG_END          = 1 << 5,
    COGL_GLES2_DIRTY_FOG_COLOR        = 1 << 6,
    COGL_GLES2_DIRTY_ALPHA_TEST_REF   = 1 << 7,
    COGL_GLES2_DIRTY_TEXTURE_UNITS    = 1 << 8,

    COGL_GLES2_DIRTY_ALL              = (1 << 9) - 1
  };

/* Dirty flags for shader vertex attribute pointers */
enum
  {
    COGL_GLES2_DIRTY_TEX_COORD_VERTEX_ATTRIB  = 1 << 0
  };

/* Dirty flags for shader vertex attributes enabled status */
enum
  {
    COGL_GLES2_DIRTY_TEX_COORD_ATTRIB_ENABLES = 1 << 0
  };

struct _CoglGles2WrapperAttributes
{
  GLint      multi_texture_coords[COGL_GLES2_MAX_TEXTURE_UNITS];
};

struct _CoglGles2WrapperUniforms
{
  GLint      mvp_matrix_uniform;
  GLint      modelview_matrix_uniform;
  GLint      texture_matrix_uniforms[COGL_GLES2_MAX_TEXTURE_UNITS];
  GLint      texture_sampler_uniforms[COGL_GLES2_MAX_TEXTURE_UNITS];

  GLint      fog_density_uniform;
  GLint      fog_start_uniform;
  GLint      fog_end_uniform;
  GLint      fog_color_uniform;

  GLint      alpha_test_ref_uniform;

  GLint      texture_unit_uniform;
};

struct _CoglGles2WrapperTexEnv
{
  GLenum texture_combine_rgb_func;
  GLenum texture_combine_rgb_src[3];
  GLenum texture_combine_rgb_op[3];

  GLenum texture_combine_alpha_func;
  GLenum texture_combine_alpha_src[3];
  GLenum texture_combine_alpha_op[3];

  GLfloat texture_combine_constant[4];
};

/* NB: We get a copy of this for each fragment/vertex
 * program varient we generate so we try to keep it
 * fairly lean */
struct _CoglGles2WrapperSettings
{
  guint32  texture_units;

  GLint    alpha_test_func;
  GLint    fog_mode;

  /* The current in-use user program */
  CoglHandle user_program;

  unsigned int alpha_test_enabled:1;
  unsigned int fog_enabled:1;

  CoglGles2WrapperTexEnv tex_env[COGL_GLES2_MAX_TEXTURE_UNITS];
};

struct _CoglGles2WrapperTextureUnit
{
  CoglMatrix   texture_matrix;

  GLenum       texture_coords_type;
  GLint	       texture_coords_size;
  GLsizei      texture_coords_stride;
  const void  *texture_coords_pointer;

  unsigned int texture_coords_enabled:1;
  unsigned int dirty_matrix:1; /*!< shader uniform needs updating */
};

struct _CoglGles2Wrapper
{
  GLuint     matrix_mode;
  CoglMatrix modelview_matrix;
  CoglMatrix projection_matrix;
  unsigned int	active_texture_unit;
  unsigned int  active_client_texture_unit;

  CoglGles2WrapperTextureUnit texture_units[COGL_GLES2_MAX_TEXTURE_UNITS];

  /* The combined modelview and projection matrix is only updated at
     the last minute in glDrawArrays to avoid recalculating it for
     every change to the modelview matrix */
  GLboolean  mvp_uptodate;

  /* The currently bound program */
  CoglGles2WrapperProgram *current_program;

  /* The current settings. Effectively these represent anything that
   * will require a modified fixed function shader */
  CoglGles2WrapperSettings settings;
  /* Whether the settings have changed since the last draw */
  gboolean settings_dirty;
  /* Uniforms that have changed since the last draw */
  int dirty_uniforms, dirty_custom_uniforms;

  /* Attribute pointers that have changed since the last draw */
  int dirty_attribute_pointers;

  /* Vertex attribute pointer enables that have changed since the last draw */
  int dirty_vertex_attrib_enables;

  /* List of all compiled program combinations */
  GSList *compiled_programs;

  /* List of all compiled vertex shaders */
  GSList *compiled_vertex_shaders;

  /* List of all compiled fragment shaders */
  GSList *compiled_fragment_shaders;

  /* Values for the uniforms */
  GLfloat alpha_test_ref;
  GLfloat fog_density;
  GLfloat fog_start;
  GLfloat fog_end;
  GLfloat fog_color[4];
  CoglBoxedValue custom_uniforms[COGL_GLES2_NUM_CUSTOM_UNIFORMS];
};

struct _CoglGles2WrapperProgram
{
  GLuint    program;

  /* The settings that were used to generate this combination */
  CoglGles2WrapperSettings settings;

  /* The attributes for this program that are not bound up-front
   * with constant indices */
  CoglGles2WrapperAttributes attributes;

  /* The uniforms for this program */
  CoglGles2WrapperUniforms uniforms;
  GLint custom_uniforms[COGL_GLES2_NUM_CUSTOM_UNIFORMS];
};

struct _CoglGles2WrapperShader
{
  GLuint shader;

  /* The settings that were used to generate this shader */
  CoglGles2WrapperSettings settings;
};

/* These defines are missing from GL ES 2 but we can still use them
   with the wrapper functions */

#ifndef GL_MODELVIEW

#define GL_MATRIX_MODE         0x0BA0

#define GL_MODELVIEW           0x1700
#define GL_PROJECTION          0x1701

#ifdef COGL_ENABLE_DEBUG
#define GL_STACK_OVERFLOW      0x0503
#define GL_STACK_UNDERFLOW     0x0504
#endif

#define GL_VERTEX_ARRAY        0x8074
#define GL_TEXTURE_COORD_ARRAY 0x8078
#define GL_COLOR_ARRAY         0x8076
#define GL_NORMAL_ARRAY        0x8075

#define GL_LIGHTING            0x0B50
#define GL_ALPHA_TEST          0x0BC0

#define GL_FOG                 0x0B60
#define GL_FOG_COLOR           0x0B66
#define GL_FOG_MODE            0x0B65
#define GL_FOG_HINT            0x0C54
#define GL_FOG_DENSITY         0x0B62
#define GL_FOG_START           0x0B63
#define GL_FOG_END             0x0B64

#define GL_CLIP_PLANE0         0x3000
#define GL_CLIP_PLANE1         0x3001
#define GL_CLIP_PLANE2         0x3002
#define GL_CLIP_PLANE3         0x3003
#define GL_MAX_CLIP_PLANES     0x0D32

#define GL_MODELVIEW_MATRIX    0x0BA6
#define GL_PROJECTION_MATRIX   0x0BA7
#define GL_TEXTURE_MATRIX      0x0BA8

#define GL_GENERATE_MIPMAP     0x8191

#define GL_TEXTURE_ENV         0x2300
#define GL_TEXTURE_ENV_MODE    0x2200
#define GL_TEXTURE_ENV_COLOR   0x2201
#define GL_MODULATE            0x2100

#define GL_EXP                 0x8000
#define GL_EXP2                0x8001

#define GL_MODULATE            0x2100
#define GL_ADD                 0x0104
#define GL_ADD_SIGNED          0x8574
#define GL_INTERPOLATE         0x8575
#define GL_SUBTRACT            0x84e7
#define GL_DOT3_RGB            0x86ae
#define GL_DOT3_RGBA           0x86af
#define GL_CONSTANT            0x8576
#define GL_PRIMARY_COLOR       0x8577
#define GL_PREVIOUS            0x8578
#define GL_COMBINE             0x8570
#define GL_COMBINE_RGB         0x8571
#define GL_COMBINE_ALPHA       0x8572
#define GL_SRC0_RGB            0x8580
#define GL_OPERAND0_RGB        0x8590
#define GL_SRC1_RGB            0x8581
#define GL_OPERAND1_RGB        0x8591
#define GL_SRC2_RGB            0x8582
#define GL_OPERAND2_RGB        0x8592
#define GL_SRC0_ALPHA          0x8588
#define GL_OPERAND0_ALPHA      0x8598
#define GL_SRC1_ALPHA          0x8589
#define GL_OPERAND1_ALPHA      0x8599
#define GL_SRC2_ALPHA          0x858a
#define GL_OPERAND2_ALPHA      0x859a
#define GL_AMBIENT             0x1200
#define GL_DIFFUSE             0x1201
#define GL_SPECULAR            0x1202
#define GL_EMISSION            0x1600
#define GL_SHININESS           0x1601

#define GL_MAX_TEXTURE_UNITS   0x84e2

#endif /* GL_MODELVIEW */

void _cogl_gles2_wrapper_init (CoglGles2Wrapper *wrapper);
void _cogl_gles2_wrapper_deinit (CoglGles2Wrapper *wrapper);

void _cogl_wrap_glPushMatrix ();
void _cogl_wrap_glPopMatrix ();
void _cogl_wrap_glMatrixMode (GLenum mode);
void _cogl_wrap_glLoadIdentity ();
void _cogl_wrap_glMultMatrixf (const GLfloat *m);
void _cogl_wrap_glLoadMatrixf (const GLfloat *m);
void _cogl_wrap_glFrustumf (GLfloat left, GLfloat right,
                            GLfloat bottom, GLfloat top,
                            GLfloat z_near, GLfloat z_far);
void _cogl_wrap_glScalef (GLfloat x, GLfloat y, GLfloat z);
void _cogl_wrap_glTranslatef (GLfloat x, GLfloat y, GLfloat z);
void _cogl_wrap_glRotatef (GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
void _cogl_wrap_glOrthof (GLfloat left, GLfloat right,
                          GLfloat bottom, GLfloat top,
                          GLfloat near, GLfloat far);

void _cogl_wrap_glEnable (GLenum cap);
void _cogl_wrap_glDisable (GLenum cap);

void _cogl_wrap_glTexCoordPointer (GLint size, GLenum type, GLsizei stride,
                                   const GLvoid *pointer);
void _cogl_wrap_glVertexPointer (GLint size, GLenum type, GLsizei stride,
                                 const GLvoid *pointer);
void _cogl_wrap_glColorPointer (GLint size, GLenum type, GLsizei stride,
                                const GLvoid *pointer);
void _cogl_wrap_glNormalPointer (GLenum type, GLsizei stride,
                                 const GLvoid *pointer);

void _cogl_wrap_glTexEnvi (GLenum target, GLenum pname, GLint param);
void _cogl_wrap_glTexEnvfv (GLenum target, GLenum pname, const GLfloat *params);

void _cogl_wrap_glClientActiveTexture (GLenum texture);
void _cogl_wrap_glActiveTexture (GLenum texture);

void _cogl_wrap_glEnableClientState (GLenum array);
void _cogl_wrap_glDisableClientState (GLenum array);

void _cogl_wrap_glAlphaFunc (GLenum func, GLclampf ref);

void _cogl_wrap_glColor4f (GLclampf r, GLclampf g, GLclampf b, GLclampf a);
void _cogl_wrap_glColor4ub (GLubyte r, GLubyte g, GLubyte b, GLubyte a);

void _cogl_wrap_glClipPlanef (GLenum plane, GLfloat *equation);

void _cogl_wrap_glGetIntegerv (GLenum pname, GLint *params);
void _cogl_wrap_glGetFloatv (GLenum pname, GLfloat *params);

void _cogl_wrap_glFogf (GLenum pname, GLfloat param);
void _cogl_wrap_glFogfv (GLenum pname, const GLfloat *params);

void _cogl_wrap_glDrawArrays (GLenum mode, GLint first, GLsizei count);
void _cogl_wrap_glDrawElements (GLenum mode, GLsizei count, GLenum type,
                                const GLvoid *indices);
void _cogl_wrap_glTexParameteri (GLenum target, GLenum pname, GLfloat param);

void _cogl_wrap_glMaterialfv (GLenum face, GLenum pname, const GLfloat *params);

/* This function is only available on GLES 2 */
#define _cogl_wrap_glGenerateMipmap glGenerateMipmap

void _cogl_gles2_clear_cache_for_program (CoglHandle program);

/* Remap the missing GL functions to use the wrappers */
#ifndef COGL_GLES2_WRAPPER_NO_REMAP
#define glDrawArrays                 _cogl_wrap_glDrawArrays
#define glDrawElements               _cogl_wrap_glDrawElements
#define glPushMatrix                 _cogl_wrap_glPushMatrix
#define glPopMatrix                  _cogl_wrap_glPopMatrix
#define glMatrixMode                 _cogl_wrap_glMatrixMode
#define glLoadIdentity               _cogl_wrap_glLoadIdentity
#define glMultMatrixf                _cogl_wrap_glMultMatrixf
#define glLoadMatrixf                _cogl_wrap_glLoadMatrixf
#define glFrustumf                   _cogl_wrap_glFrustumf
#define glScalef                     _cogl_wrap_glScalef
#define glTranslatef                 _cogl_wrap_glTranslatef
#define glRotatef                    _cogl_wrap_glRotatef
#define glOrthof                     _cogl_wrap_glOrthof
#define glEnable                     _cogl_wrap_glEnable
#define glDisable                    _cogl_wrap_glDisable
#define glTexCoordPointer            _cogl_wrap_glTexCoordPointer
#define glVertexPointer              _cogl_wrap_glVertexPointer
#define glColorPointer               _cogl_wrap_glColorPointer
#define glNormalPointer              _cogl_wrap_glNormalPointer
#define glTexEnvi                    _cogl_wrap_glTexEnvi
#define glTexEnvfv                   _cogl_wrap_glTexEnvfv
#define glActiveTexture              _cogl_wrap_glActiveTexture
#define glClientActiveTexture        _cogl_wrap_glClientActiveTexture
#define glEnableClientState          _cogl_wrap_glEnableClientState
#define glDisableClientState         _cogl_wrap_glDisableClientState
#define glAlphaFunc                  _cogl_wrap_glAlphaFunc
#define glColor4f                    _cogl_wrap_glColor4f
#define glColor4ub                   _cogl_wrap_glColor4ub
#define glClipPlanef                 _cogl_wrap_glClipPlanef
#define glGetIntegerv                _cogl_wrap_glGetIntegerv
#define glGetFloatv                  _cogl_wrap_glGetFloatv
#define glFogf                       _cogl_wrap_glFogf
#define glFogfv                      _cogl_wrap_glFogfv
#define glTexParameteri              _cogl_wrap_glTexParameteri
#define glMaterialfv                 _cogl_wrap_glMaterialfv
#endif /* COGL_GLES2_WRAPPER_NO_REMAP */

#else /* HAVE_COGL_GLES2 */

/* COGL uses the automatic mipmap generation for GLES 1 so
   glGenerateMipmap doesn't need to do anything */
#define _cogl_wrap_glGenerateMipmap(x) ((void) 0)

/* GLES doesn't have glDrawRangeElements, so we simply pretend it does
 * but that it makes no use of the start, end constraints: */
#define glDrawRangeElements(mode, start, end, count, type, indices) \
  glDrawElements (mode, count, type, indices)

#endif /* HAVE_COGL_GLES2 */

G_END_DECLS

#endif /* __COGL_GLES2_WRAPPER_H__ */
