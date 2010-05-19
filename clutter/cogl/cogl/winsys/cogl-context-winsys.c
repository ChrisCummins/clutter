/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010 Intel Corporation.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-context.h"

void
_cogl_create_context_winsys (CoglContext *context)
{
#ifdef COGL_HAS_XLIB_SUPPORT
  context->winsys.event_filters = NULL;
#endif
}

#ifdef COGL_HAS_XLIB_SUPPORT

#include "cogl-xlib.h"

static void
free_xlib_filter_closure (gpointer data, gpointer user_data)
{
  g_slice_free (CoglXlibFilterClosure, data);
}

#endif

void
_cogl_destroy_context_winsys (CoglContext *context)
{
#ifdef COGL_HAS_XLIB_SUPPORT
  g_slist_foreach (context->winsys.event_filters,
                   free_xlib_filter_closure, NULL);
  g_slist_free (context->winsys.event_filters);
#endif
}
