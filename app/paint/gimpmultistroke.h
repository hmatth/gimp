/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpmultistroke.h
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

#ifndef __GIMP_MULTI_STROKE_H__
#define __GIMP_MULTI_STROKE_H__


#include "core/gimpobject.h"


#define GIMP_TYPE_MULTI_STROKE            (gimp_multi_stroke_get_type ())
#define GIMP_MULTI_STROKE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_MULTI_STROKE, GimpMultiStroke))
#define GIMP_MULTI_STROKE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_MULTI_STROKE, GimpMultiStrokeClass))
#define GIMP_IS_MULTI_STROKE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_MULTI_STROKE))
#define GIMP_IS_MULTI_STROKE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_MULTI_STROKE))
#define GIMP_MULTI_STROKE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_MULTI_STROKE, GimpMultiStrokeClass))

typedef struct _GimpMultiStrokeClass   GimpMultiStrokeClass;

struct _GimpMultiStroke
{
  GimpObject  parent_instance;

  GimpImage    *image;
  GimpDrawable *drawable;
  GimpCoords   *origin;

  GList        *strokes;

  GType         type;
};

struct _GimpMultiStrokeClass
{
  GimpObjectClass  parent_class;

  const gchar * label;

  /* Virtual functions */
  void       (* update_strokes)             (GimpMultiStroke *mstroke,
                                             GimpDrawable    *drawable,
                                             GimpCoords      *origin);
  GeglNode * (* get_operation)              (GimpMultiStroke *mstroke,
                                             gint             stroke,
                                             gint             paint_width,
                                             gint             paint_height);
  GParamSpec **
             (* get_settings)               (GimpMultiStroke *mstroke,
                                             guint           *nproperties);
  GParamSpec **
             (* get_xcf_settings)           (GimpMultiStroke *mstroke,
                                             guint           *nproperties);
};


GType        gimp_multi_stroke_get_type      (void) G_GNUC_CONST;

void         gimp_multi_stroke_set_origin    (GimpMultiStroke *mstroke,
                                              GimpDrawable    *drawable,
                                              GimpCoords      *origin);

GimpCoords * gimp_multi_stroke_get_origin    (GimpMultiStroke *mstroke);
gint         gimp_multi_stroke_get_size      (GimpMultiStroke *mstroke);
GimpCoords * gimp_multi_stroke_get_coords    (GimpMultiStroke *mstroke,
                                              gint             stroke);
GeglNode   * gimp_multi_stroke_get_operation (GimpMultiStroke *mstroke,
                                              gint             stroke,
                                              gint             paint_width,
                                              gint             paint_height);
GParamSpec ** gimp_multi_stroke_get_settings  (GimpMultiStroke *mstroke,
                                               guint           *nproperties);
GParamSpec **
           gimp_multi_stroke_get_xcf_settings (GimpMultiStroke *mstroke,
                                               guint           *nproperties);

#endif  /*  __GIMP_MULTI_STROKE_H__  */

