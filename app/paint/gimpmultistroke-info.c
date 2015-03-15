/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpmultistroke-info.c
 * Copyright (C) 2015 Jehan <jehan@gimp.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gegl.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "paint-types.h"

#include "gimpmirror.h"
#include "gimpmultistroke.h"
#include "gimpmultistroke-info.h"
#include "gimptiling.h"

GList *
gimp_multi_stroke_list (void)
{
  GList *list = NULL;

  list = g_list_prepend (list, GINT_TO_POINTER (GIMP_TYPE_MIRROR));
  list = g_list_prepend (list, GINT_TO_POINTER (GIMP_TYPE_TILING));
  return list;
}

GimpMultiStroke *
gimp_multi_stroke_new (GType      type,
                       GimpImage *image)
{
  GimpMultiStroke *mstroke = NULL;

  g_return_val_if_fail (g_type_is_a (type, GIMP_TYPE_MULTI_STROKE),
                        NULL);

  if (type != G_TYPE_NONE)
    {
      mstroke = g_object_new (type,
                              "image", image,
                              NULL);
      mstroke->type = type;
    }

  return mstroke;
}
