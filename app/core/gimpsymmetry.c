/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpsymmetry.c
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

#include "core-types.h"

#include "gimpdrawable.h"
#include "gimpimage.h"
#include "gimpitem.h"
#include "gimpsymmetry.h"

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

static void gimp_symmetry_finalize        (GObject      *object);
static void gimp_symmetry_set_property    (GObject      *object,
                                           guint         property_id,
                                           const GValue *value,
                                           GParamSpec   *pspec);
static void gimp_symmetry_get_property    (GObject      *object,
                                           guint         property_id,
                                           GValue       *value,
                                           GParamSpec   *pspec);

static void
        gimp_symmetry_real_update_strokes (GimpSymmetry *sym,
                                           GimpDrawable *drawable,
                                           GimpCoords   *origin);
static GeglNode *
            gimp_symmetry_real_get_op     (GimpSymmetry *sym,
                                           gint          stroke,
                                           gint          paint_width,
                                           gint          paint_height);
static GParamSpec **
          gimp_symmetry_real_get_settings (GimpSymmetry *sym,
                                           guint        *n_properties);

G_DEFINE_TYPE (GimpSymmetry, gimp_symmetry, GIMP_TYPE_OBJECT)

#define parent_class gimp_symmetry_parent_class

static guint gimp_symmetry_signals[LAST_SIGNAL] = { 0 };

static void
gimp_symmetry_class_init (GimpSymmetryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /* This signal should likely be emitted at the end of update_strokes()
   * if stroke coordinates were changed. */
  gimp_symmetry_signals[STROKES_UPDATED] =
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
  gimp_symmetry_signals[UPDATE_UI] =
    g_signal_new ("update-ui",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  1, GIMP_TYPE_IMAGE);


  object_class->finalize     = gimp_symmetry_finalize;
  object_class->set_property = gimp_symmetry_set_property;
  object_class->get_property = gimp_symmetry_get_property;

  klass->label               = "None";
  klass->update_strokes      = gimp_symmetry_real_update_strokes;
  klass->get_operation       = gimp_symmetry_real_get_op;
  klass->get_settings        = gimp_symmetry_real_get_settings;
  klass->get_xcf_settings    = gimp_symmetry_real_get_settings;

  g_object_class_install_property (object_class, PROP_IMAGE,
                                   g_param_spec_object ("image",
                                                        NULL, NULL,
                                                        GIMP_TYPE_IMAGE,
                                                        GIMP_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY));
}


static void
gimp_symmetry_init (GimpSymmetry *sym)
{
  sym->image    = NULL;
  sym->drawable = NULL;
  sym->origin   = NULL;
  sym->strokes  = NULL;
  sym->type     = G_TYPE_NONE;
}

static void
gimp_symmetry_finalize (GObject *object)
{
  GimpSymmetry *sym = GIMP_SYMMETRY (object);

  if (sym->drawable)
    g_object_unref (sym->drawable);

  g_free (sym->origin);
  sym->origin = NULL;

  g_list_free_full (sym->strokes, g_free);
  sym->strokes = NULL;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gimp_symmetry_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GimpSymmetry *sym = GIMP_SYMMETRY (object);

  switch (property_id)
    {
    case PROP_IMAGE:
      sym->image = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_symmetry_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GimpSymmetry *sym = GIMP_SYMMETRY (object);

  switch (property_id)
    {
    case PROP_IMAGE:
      g_value_set_object (value, sym->image);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_symmetry_real_update_strokes (GimpSymmetry *sym,
                                   GimpDrawable *drawable,
                                   GimpCoords   *origin)
{
  /* The basic symmetry just uses the origin as is. */
  sym->strokes = g_list_prepend (sym->strokes,
                                     g_memdup (origin, sizeof (GimpCoords)));
}

static GeglNode *
gimp_symmetry_real_get_op (GimpSymmetry *sym,
                           gint          stroke,
                           gint          paint_width,
                           gint          paint_height)
{
  /* The basic symmetry just returns NULL, since no transformation of the
   * brush painting happen. */
  return NULL;
}

static GParamSpec **
gimp_symmetry_real_get_settings (GimpSymmetry *sym,
                                 guint        *n_properties)
{
  *n_properties = 0;

  return NULL;
}

/***** Public Functions *****/

/**
 * gimp_symmetry_set_origin:
 * @sym:      the #GimpSymmetry
 * @drawable: the #GimpDrawable where painting will happen
 * @origin:   new base coordinates.
 *
 * Set the symmetry to new origin coordinates and drawable.
 **/
void
gimp_symmetry_set_origin (GimpSymmetry *sym,
                          GimpDrawable *drawable,
                          GimpCoords   *origin)
{
  g_return_if_fail (GIMP_IS_SYMMETRY (sym));
  g_return_if_fail (GIMP_IS_DRAWABLE (drawable));
  g_return_if_fail (gimp_item_get_image (GIMP_ITEM (drawable)) == sym->image);

  if (drawable != sym->drawable)
    {
      if (sym->drawable)
        g_object_unref (sym->drawable);
      sym->drawable = g_object_ref (drawable);
    }

  if (origin != sym->origin)
    {
      g_free (sym->origin);
      sym->origin = g_memdup (origin, sizeof (GimpCoords));
    }

  g_list_free_full (sym->strokes, g_free);
  sym->strokes = NULL;

  GIMP_SYMMETRY_GET_CLASS (sym)->update_strokes (sym,
                                                 drawable,
                                                 origin);
}

/**
 * gimp_symmetry_get_origin:
 * @sym: the #GimpSymmetry
 *
 * Returns the origin stroke coordinates.
 * The returned value is owned by the #GimpSymmetry and must not be freed.
 **/
GimpCoords *
gimp_symmetry_get_origin (GimpSymmetry *sym)
{
  g_return_val_if_fail (GIMP_IS_SYMMETRY (sym), NULL);

  return sym->origin;
}

/**
 * gimp_symmetry_get_size:
 * @sym: the #GimpSymmetry
 *
 * Returns the total number of strokes.
 **/
gint
gimp_symmetry_get_size (GimpSymmetry *sym)
{
  g_return_val_if_fail (GIMP_IS_SYMMETRY (sym), 0);

  return g_list_length (sym->strokes);
}

/**
 * gimp_symmetry_get_coords:
 * @sym:    the #GimpSymmetry
 * @stroke: the stroke number
 *
 * Returns the coordinates of the stroke number @stroke.
 * The returned value is owned by the #GimpSymmetry and must not be freed.
 **/
GimpCoords *
gimp_symmetry_get_coords (GimpSymmetry *sym,
                          gint          stroke)
{
  g_return_val_if_fail (GIMP_IS_SYMMETRY (sym), NULL);

  return g_list_nth_data (sym->strokes, stroke);
}

/**
 * gimp_symmetry_get_operation:
 * @sym:          the #GimpSymmetry
 * @stroke:       the stroke number
 * @paint_width:  the width of the painting area
 * @paint_height: the height of the painting area
 *
 * Returns the operation to apply to the paint buffer for stroke number @stroke.
 * NULL means to copy the original stroke as-is.
 **/
GeglNode *
gimp_symmetry_get_operation (GimpSymmetry *sym,
                             gint          stroke,
                             gint          paint_width,
                             gint          paint_height)
{
  g_return_val_if_fail (GIMP_IS_SYMMETRY (sym), NULL);

  return GIMP_SYMMETRY_GET_CLASS (sym)->get_operation (sym,
                                                       stroke,
                                                       paint_width,
                                                       paint_height);
}

/**
 * gimp_symmetry_get_settings:
 * @sym:     the #GimpSymmetry
 * @n_properties: the number of properties in the returned array
 *
 * Returns an array of the symmetry properties which are supposed to
 * be settable by the user.
 * The returned array must be freed by the caller.
 **/
GParamSpec **
gimp_symmetry_get_settings (GimpSymmetry *sym,
                            guint        *n_properties)
{
  g_return_val_if_fail (GIMP_IS_SYMMETRY (sym), NULL);

  return GIMP_SYMMETRY_GET_CLASS (sym)->get_settings (sym,
                                                      n_properties);
}

/**
 * gimp_symmetry_get_xcf_settings:
 * @sym:         the #GimpSymmetry
 * @n_properties: the number of properties in the returned array
 *
 * Returns an array of the symmetry properties which are to be serialized
 * when saving to XCF.
 * These properties may be different to `gimp_symmetry_get_settings()`
 * (for instance some properties are not meant to be user-changeable but
 * still saved)
 * The returned array must be freed by the caller.
 **/
GParamSpec **
gimp_symmetry_get_xcf_settings (GimpSymmetry *sym,
                                guint        *n_properties)
{
  g_return_val_if_fail (GIMP_IS_SYMMETRY (sym), NULL);

  return GIMP_SYMMETRY_GET_CLASS (sym)->get_xcf_settings (sym,
                                                          n_properties);
}
