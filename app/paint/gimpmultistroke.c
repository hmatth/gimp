/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpmultistroke.c
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

#include <string.h>

#include <gegl.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "paint-types.h"

#include "core/gimpdrawable.h"
#include "core/gimpimage.h"
#include "core/gimpitem.h"

#include "gimpmultistroke.h"

#include "gimp-intl.h"

enum
{
  STROKES_UPDATED,
  UPDATE_UI,
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_IMAGE,
};

/* Local function prototypes */

static void gimp_multi_stroke_finalize        (GObject         *object);
static void gimp_multi_stroke_set_property    (GObject         *object,
                                               guint            property_id,
                                               const GValue    *value,
                                               GParamSpec      *pspec);
static void gimp_multi_stroke_get_property    (GObject         *object,
                                               guint            property_id,
                                               GValue          *value,
                                               GParamSpec      *pspec);

static void
        gimp_multi_stroke_real_update_strokes (GimpMultiStroke *mstroke,
                                               GimpDrawable    *drawable,
                                               GimpCoords      *origin);
static GeglNode *
            gimp_multi_stroke_real_get_op     (GimpMultiStroke *mstroke,
                                               gint             stroke,
                                               gint             paint_width,
                                               gint             paint_height);
static GParamSpec **
            gimp_multi_stroke_real_get_settings (GimpMultiStroke *mstroke,
                                                 guint           *nproperties);

G_DEFINE_TYPE (GimpMultiStroke, gimp_multi_stroke, GIMP_TYPE_OBJECT)

#define parent_class gimp_multi_stroke_parent_class

static guint gimp_multi_stroke_signals[LAST_SIGNAL] = { 0 };

static void
gimp_multi_stroke_class_init (GimpMultiStrokeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /* This signal should likely be emitted at the end of update_strokes()
   * if stroke coordinates were changed. */
  gimp_multi_stroke_signals[STROKES_UPDATED] =
    g_signal_new ("strokes-updated",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  1, GIMP_TYPE_IMAGE);
  /* This signal should be emitted when you request a change in
   * the settings UI. For instance adding some settings (therefore having a
   * dynamic UI), or changing scale min/max extremes, etc. */
  gimp_multi_stroke_signals[UPDATE_UI] =
    g_signal_new ("update-ui",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  1, GIMP_TYPE_IMAGE);


  object_class->finalize     = gimp_multi_stroke_finalize;
  object_class->set_property = gimp_multi_stroke_set_property;
  object_class->get_property = gimp_multi_stroke_get_property;

  klass->label               = "None";
  klass->update_strokes      = gimp_multi_stroke_real_update_strokes;
  klass->get_operation       = gimp_multi_stroke_real_get_op;
  klass->get_settings        = gimp_multi_stroke_real_get_settings;
  klass->get_xcf_settings    = gimp_multi_stroke_real_get_settings;

  g_object_class_install_property (object_class, PROP_IMAGE,
                                   g_param_spec_object ("image",
                                                        NULL, NULL,
                                                        GIMP_TYPE_IMAGE,
                                                        GIMP_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY));
}


static void
gimp_multi_stroke_init (GimpMultiStroke *mstroke)
{
  mstroke->image    = NULL;
  mstroke->drawable = NULL;
  mstroke->origin   = NULL;
  mstroke->strokes  = NULL;
  mstroke->type     = G_TYPE_NONE;
}

static void
gimp_multi_stroke_finalize (GObject *object)
{
  GimpMultiStroke *mstroke = GIMP_MULTI_STROKE (object);

  if (mstroke->drawable)
    g_object_unref (mstroke->drawable);

  g_free (mstroke->origin);
  mstroke->origin = NULL;

  g_list_free_full (mstroke->strokes, g_free);
  mstroke->strokes = NULL;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gimp_multi_stroke_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GimpMultiStroke *mstroke = GIMP_MULTI_STROKE (object);

  switch (property_id)
    {
    case PROP_IMAGE:
      mstroke->image = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_multi_stroke_get_property (GObject    *object,
                                guint       property_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GimpMultiStroke *mstroke = GIMP_MULTI_STROKE (object);

  switch (property_id)
    {
    case PROP_IMAGE:
      g_value_set_object (value, mstroke->image);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_multi_stroke_real_update_strokes (GimpMultiStroke *mstroke,
                                       GimpDrawable    *drawable,
                                       GimpCoords      *origin)
{
  /* The basic multi-stroke just uses the origin as is. */
  mstroke->strokes = g_list_prepend (mstroke->strokes,
                                     g_memdup (origin, sizeof (GimpCoords)));
}

static GeglNode *
gimp_multi_stroke_real_get_op (GimpMultiStroke *mstroke,
                               gint             stroke,
                               gint             paint_width,
                               gint             paint_height)
{
  /* The basic multi-stroke just returns NULL, since no transformation of the
   * brush painting happen. */
  return NULL;
}

static GParamSpec **
gimp_multi_stroke_real_get_settings (GimpMultiStroke *mstroke,
                                     guint           *nproperties)
{
  *nproperties = 0;

  return NULL;
}

/***** Public Functions *****/

/**
 * gimp_multi_stroke_set_origin:
 * @mstroke: the #GimpMultiStroke
 * @drawable:     the #GimpDrawable where painting will happen
 * @origin:       new base coordinates.
 *
 * Set the multi-stroke to new origin coordinates and drawable.
 **/
void
gimp_multi_stroke_set_origin (GimpMultiStroke *mstroke,
                              GimpDrawable    *drawable,
                              GimpCoords      *origin)
{
  g_return_if_fail (GIMP_IS_MULTI_STROKE (mstroke));
  g_return_if_fail (GIMP_IS_DRAWABLE (drawable));
  g_return_if_fail (gimp_item_get_image (GIMP_ITEM (drawable)) == mstroke->image);

  if (drawable != mstroke->drawable)
    {
      if (mstroke->drawable)
        g_object_unref (mstroke->drawable);
      mstroke->drawable = g_object_ref (drawable);
    }

  if (origin != mstroke->origin)
    {
      g_free (mstroke->origin);
      mstroke->origin = g_memdup (origin, sizeof (GimpCoords));
    }

  g_list_free_full (mstroke->strokes, g_free);
  mstroke->strokes = NULL;

  GIMP_MULTI_STROKE_GET_CLASS (mstroke)->update_strokes (mstroke,
                                                         drawable,
                                                         origin);
}

/**
 * gimp_multi_stroke_get_origin:
 * @mstroke: the #GimpMultiStroke
 *
 * Returns the origin stroke coordinates.
 * The returned value is owned by the #GimpMultiStroke and must not be freed.
 **/
GimpCoords *
gimp_multi_stroke_get_origin (GimpMultiStroke *mstroke)
{
  g_return_val_if_fail (GIMP_IS_MULTI_STROKE (mstroke), NULL);

  return mstroke->origin;
}

/**
 * gimp_multi_stroke_get_size:
 * @mstroke: the #GimpMultiStroke
 *
 * Returns the total number of strokes.
 **/
gint
gimp_multi_stroke_get_size (GimpMultiStroke *mstroke)
{
  g_return_val_if_fail (GIMP_IS_MULTI_STROKE (mstroke), 0);

  return g_list_length (mstroke->strokes);
}

/**
 * gimp_multi_stroke_get_coords:
 * @mstroke: the #GimpMultiStroke
 * @stroke:       the stroke number
 *
 * Returns the coordinates of the stroke number @stroke.
 * The returned value is owned by the #GimpMultiStroke and must not be freed.
 **/
GimpCoords *
gimp_multi_stroke_get_coords (GimpMultiStroke *mstroke,
                              gint             stroke)
{
  g_return_val_if_fail (GIMP_IS_MULTI_STROKE (mstroke), NULL);

  return g_list_nth_data (mstroke->strokes, stroke);
}

/**
 * gimp_multi_stroke_get_operation:
 * @mstroke:      the #GimpMultiStroke
 * @paint_buffer: the #GeglBuffer normally used for @stroke
 * @stroke:       the stroke number
 *
 * Returns the operation to apply to the paint buffer for stroke number @stroke.
 * NULL means to copy the original stroke as-is.
 **/
GeglNode *
gimp_multi_stroke_get_operation (GimpMultiStroke *mstroke,
                                 gint             stroke,
                                 gint             paint_width,
                                 gint             paint_height)
{
  g_return_val_if_fail (GIMP_IS_MULTI_STROKE (mstroke), NULL);

  return GIMP_MULTI_STROKE_GET_CLASS (mstroke)->get_operation (mstroke,
                                                               stroke,
                                                               paint_width,
                                                               paint_height);
}

/**
 * gimp_multi_stroke_get_settings:
 * @mstroke:     the #GimpMultiStroke
 * @nproperties: the number of properties in the returned array
 *
 * Returns an array of the Multi-Stroke properties which are supposed to
 * be settable by the user.
 * The returned array must be freed by the caller.
 **/
GParamSpec **
gimp_multi_stroke_get_settings (GimpMultiStroke *mstroke,
                                guint           *nproperties)
{
  g_return_val_if_fail (GIMP_IS_MULTI_STROKE (mstroke), NULL);

  return GIMP_MULTI_STROKE_GET_CLASS (mstroke)->get_settings (mstroke,
                                                              nproperties);
}

/**
 * gimp_multi_stroke_get_xcf_settings:
 * @mstroke:     the #GimpMultiStroke
 * @nproperties: the number of properties in the returned array
 *
 * Returns an array of the Multi-Stroke properties which are to be serialized
 * when saving to XCF.
 * These properties may be different to `gimp_multi_stroke_get_settings()`
 * (for instance some properties are not meant to be user-changeable but
 * still saved)
 * The returned array must be freed by the caller.
 **/
GParamSpec **
gimp_multi_stroke_get_xcf_settings (GimpMultiStroke *mstroke,
                                    guint           *nproperties)
{
  g_return_val_if_fail (GIMP_IS_MULTI_STROKE (mstroke), NULL);

  return GIMP_MULTI_STROKE_GET_CLASS (mstroke)->get_xcf_settings (mstroke,
                                                                  nproperties);
}
