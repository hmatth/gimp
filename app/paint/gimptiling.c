/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimptiling.c
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

#include "libgimpconfig/gimpconfig.h"

#include "paint-types.h"

#include "core/gimp.h"
#include "core/gimpdrawable.h"
#include "core/gimpimage.h"
#include "core/gimpitem.h"

#include "gimptiling.h"

#include "gimp-intl.h"

/* Using same epsilon as in GLIB. */
#define G_DOUBLE_EPSILON        (1e-90)

enum
{
  PROP_0,

  PROP_X_INTERVAL,
  PROP_Y_INTERVAL,
  PROP_SHIFT,
  PROP_X_MAX,
  PROP_Y_MAX
};

/* Local function prototypes */

static void       gimp_tiling_constructed        (GObject         *object);
static void       gimp_tiling_finalize           (GObject         *object);
static void       gimp_tiling_set_property       (GObject         *object,
                                                  guint            property_id,
                                                  const GValue    *value,
                                                  GParamSpec      *pspec);
static void       gimp_tiling_get_property       (GObject         *object,
                                                  guint            property_id,
                                                  GValue          *value,
                                                  GParamSpec      *pspec);

static void       gimp_tiling_update_strokes     (GimpMultiStroke *tiling,
                                                  GimpDrawable    *drawable,
                                                  GimpCoords      *origin);
static GeglNode * gimp_tiling_get_operation      (GimpMultiStroke *tiling,
                                                  gint             stroke,
                                                  gint             paint_width,
                                                  gint             paint_height);
static GParamSpec ** gimp_tiling_get_settings    (GimpMultiStroke *mstroke,
                                                  guint           *nsettings);
static void
               gimp_tiling_image_size_changed_cb (GimpImage       *image ,
                                                  gint             previous_origin_x,
                                                  gint             previous_origin_y,
                                                  gint             previous_width,
                                                  gint             previous_height,
                                                  GimpMultiStroke *mstroke);

G_DEFINE_TYPE (GimpTiling, gimp_tiling, GIMP_TYPE_MULTI_STROKE)

#define parent_class gimp_tiling_parent_class

static void
gimp_tiling_class_init (GimpTilingClass *klass)
{
  GObjectClass         *object_class       = G_OBJECT_CLASS (klass);
  GimpMultiStrokeClass *multi_stroke_class = GIMP_MULTI_STROKE_CLASS (klass);

  object_class->constructed            = gimp_tiling_constructed;
  object_class->finalize               = gimp_tiling_finalize;
  object_class->set_property           = gimp_tiling_set_property;
  object_class->get_property           = gimp_tiling_get_property;

  multi_stroke_class->label            = "Tiling";
  multi_stroke_class->update_strokes   = gimp_tiling_update_strokes;
  multi_stroke_class->get_operation    = gimp_tiling_get_operation;
  multi_stroke_class->get_settings     = gimp_tiling_get_settings;
  multi_stroke_class->get_xcf_settings = gimp_tiling_get_settings;

  GIMP_CONFIG_INSTALL_PROP_DOUBLE (object_class, PROP_X_INTERVAL,
                                   "x-interval", _("Intervals on x-axis (pixels)"),
                                   0.0, 10000.0, 0.0,
                                   GIMP_PARAM_STATIC_STRINGS);
  GIMP_CONFIG_INSTALL_PROP_DOUBLE (object_class, PROP_Y_INTERVAL,
                                   "y-interval", _("Intervals on y-axis (pixels)"),
                                   0.0, 10000.0, 0.0,
                                   GIMP_PARAM_STATIC_STRINGS);
  GIMP_CONFIG_INSTALL_PROP_DOUBLE (object_class, PROP_SHIFT,
                                   "shift", _("X-shift between lines (pixels)"),
                                   0.0, 10000.0, 0.0,
                                   GIMP_PARAM_STATIC_STRINGS);
  GIMP_CONFIG_INSTALL_PROP_UINT (object_class, PROP_X_MAX,
                                 "x-max", _("Max strokes on x-axis"),
                                 0, 100, 0,
                                 GIMP_PARAM_STATIC_STRINGS);
  GIMP_CONFIG_INSTALL_PROP_UINT (object_class, PROP_Y_MAX,
                                 "y-max", _("Max strokes on y-axis"),
                                 0, 100, 0,
                                 GIMP_PARAM_STATIC_STRINGS);
}

static void
gimp_tiling_init (GimpTiling *tiling)
{
  tiling->interval_x = 0.0;
  tiling->interval_y = 0.0;
  tiling->shift      = 0.0;
}

static void
gimp_tiling_constructed (GObject *object)
{
  GimpMultiStroke  *mstroke;
  GParamSpecDouble *dspec;

  mstroke = GIMP_MULTI_STROKE (object);

  // TODO: can I have it in property and still save it in parent? Test.
  // Also connect on image changing size.
  // Connect to either GimpImage::size-changed-detailed
  // or GimpViewable::size-changed
  // see gimp_image_real_size_changed_detailed
  /* Update property values to actual image size. */
  dspec = G_PARAM_SPEC_DOUBLE (g_object_class_find_property (G_OBJECT_GET_CLASS (object),
                                                             "x-interval"));
  dspec->maximum = gimp_image_get_width (mstroke->image);

  dspec = G_PARAM_SPEC_DOUBLE (g_object_class_find_property (G_OBJECT_GET_CLASS (object),
                                                             "shift"));
  dspec->maximum = gimp_image_get_width (mstroke->image);

  dspec = G_PARAM_SPEC_DOUBLE (g_object_class_find_property (G_OBJECT_GET_CLASS (object),
                                                             "y-interval"));
  dspec->maximum = gimp_image_get_height (mstroke->image);

  g_signal_connect (mstroke->image, "size-changed-detailed",
                    G_CALLBACK (gimp_tiling_image_size_changed_cb),
                    mstroke);
}

static void
gimp_tiling_finalize (GObject *object)
{
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gimp_tiling_set_property (GObject      *object,
                          guint         property_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  GimpTiling      *tiling = GIMP_TILING (object);
  GimpMultiStroke *mstroke = GIMP_MULTI_STROKE (tiling);

  switch (property_id)
    {
    case PROP_X_INTERVAL:
      if (mstroke->image)
        {
          gdouble new_x = g_value_get_double (value);

          if (new_x < gimp_image_get_width (mstroke->image))
            {
              tiling->interval_x = new_x;

              if (tiling->interval_x <= tiling->shift + G_DOUBLE_EPSILON)
                {
                  GValue val = {0,};

                  g_value_init (&val, G_TYPE_DOUBLE);
                  g_value_set_double (&val, 0.0);
                  g_object_set_property (G_OBJECT (object), "shift", &val);
                }
              else if (mstroke->drawable)
                gimp_tiling_update_strokes (mstroke, mstroke->drawable, mstroke->origin);
            }
        }
      break;
    case PROP_Y_INTERVAL:
        {
          gdouble new_y = g_value_get_double (value);

          if (new_y < gimp_image_get_height (mstroke->image))
            {
              tiling->interval_y = new_y;

              if (tiling->interval_y <= G_DOUBLE_EPSILON)
                {
                  GValue val = {0,};

                  g_value_init (&val, G_TYPE_DOUBLE);
                  g_value_set_double (&val, 0.0);
                  g_object_set_property (G_OBJECT (object), "shift", &val);
                }
              else if (mstroke->drawable)
                gimp_tiling_update_strokes (mstroke, mstroke->drawable, mstroke->origin);
            }
        }
      break;
    case PROP_SHIFT:
        {
          gdouble new_shift = g_value_get_double (value);

          if (new_shift == 0.0 ||
              (tiling->interval_y != 0.0 && new_shift < tiling->interval_x))
            {
              tiling->shift = new_shift;
              if (mstroke->drawable)
                gimp_tiling_update_strokes (mstroke, mstroke->drawable, mstroke->origin);
            }
        }
      break;
    case PROP_X_MAX:
      tiling->max_x = g_value_get_uint (value);
      if (mstroke->drawable)
        gimp_tiling_update_strokes (mstroke, mstroke->drawable, mstroke->origin);
      break;
    case PROP_Y_MAX:
      tiling->max_y = g_value_get_uint (value);
      if (mstroke->drawable)
        gimp_tiling_update_strokes (mstroke, mstroke->drawable, mstroke->origin);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_tiling_get_property (GObject    *object,
                          guint       property_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GimpTiling *tiling = GIMP_TILING (object);

  switch (property_id)
    {
    case PROP_X_INTERVAL:
      g_value_set_double (value, tiling->interval_x);
      break;
    case PROP_Y_INTERVAL:
      g_value_set_double (value, tiling->interval_y);
      break;
    case PROP_SHIFT:
      g_value_set_double (value, tiling->shift);
      break;
    case PROP_X_MAX:
      g_value_set_uint (value, tiling->max_x);
      break;
    case PROP_Y_MAX:
      g_value_set_uint (value, tiling->max_y);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_tiling_update_strokes (GimpMultiStroke *mstroke,
                            GimpDrawable    *drawable,
                            GimpCoords      *origin)
{
  GList            *strokes = NULL;
  GimpTiling       *tiling  = GIMP_TILING (mstroke);
  GimpCoords       *coords;
  gint              width;
  gint              height;
  gint              startx = origin->x;
  gint              starty = origin->y;
  gint              x;
  gint              y;
  gint              x_count;
  gint              y_count;

  g_return_if_fail (GIMP_IS_DRAWABLE (drawable));
  g_return_if_fail (GIMP_IS_MULTI_STROKE (mstroke));

  g_list_free_full (mstroke->strokes, g_free);
  mstroke->strokes = NULL;

  width  = gimp_item_get_width (GIMP_ITEM (drawable));
  height = gimp_item_get_height (GIMP_ITEM (drawable));

  if (origin->x > 0 && tiling->max_x == 0)
    startx = origin->x - tiling->interval_x * (gint) (origin->x / tiling->interval_x + 1)
             - tiling->shift * (gint) (origin->y / tiling->interval_y + 1);
  if (origin->y > 0 && tiling->max_y == 0)
    starty = origin->y - tiling->interval_y * (gint) (origin->y / tiling->interval_y + 1);
  for (y_count = 0, y = starty; y < height + tiling->interval_y;
       y_count++, y += tiling->interval_y)
    {
      for (x_count = 0, x = startx; x < width + tiling->interval_x;
           x_count++, x += tiling->interval_x)
        {
          coords = g_memdup (origin, sizeof (GimpCoords));
          coords->x = x;
          coords->y = y;
          strokes = g_list_prepend (strokes, coords);
          if (tiling->interval_x < 1.0 ||
              (tiling->max_x && x_count >= (gint) tiling->max_x))
            break;
        }
      if (tiling->interval_y < 1.0 ||
          (tiling->max_y && y_count >= (gint) tiling->max_y))
        break;
      if (tiling->max_x || startx + tiling->shift <= 0.0)
        startx = startx + tiling->shift;
      else
        startx = startx - tiling->interval_x + tiling->shift;
    }
  mstroke->strokes = strokes;

  g_signal_emit_by_name (mstroke, "strokes-updated", mstroke->image);
}

static GeglNode *
gimp_tiling_get_operation (GimpMultiStroke *mstroke,
                           gint             stroke,
                           gint             paint_width,
                           gint             paint_height)
{
  /* No buffer transformation happens for tiling. */
  return NULL;
}

static GParamSpec **
gimp_tiling_get_settings (GimpMultiStroke *mstroke,
                          guint           *nsettings)
{
  GParamSpec **pspecs;

  *nsettings = 6;
  pspecs = g_new (GParamSpec*, 6);

  pspecs[0] = g_object_class_find_property (G_OBJECT_GET_CLASS (mstroke),
                                            "x-interval");
  pspecs[1] = g_object_class_find_property (G_OBJECT_GET_CLASS (mstroke),
                                            "y-interval");
  pspecs[2] = g_object_class_find_property (G_OBJECT_GET_CLASS (mstroke),
                                            "shift");
  pspecs[3] = NULL;
  pspecs[4] = g_object_class_find_property (G_OBJECT_GET_CLASS (mstroke),
                                            "x-max");
  pspecs[5] = g_object_class_find_property (G_OBJECT_GET_CLASS (mstroke),
                                            "y-max");

  return pspecs;
}

static void
gimp_tiling_image_size_changed_cb (GimpImage       *image,
                                   gint             previous_origin_x,
                                   gint             previous_origin_y,
                                   gint             previous_width,
                                   gint             previous_height,
                                   GimpMultiStroke *mstroke)
{
  GParamSpecDouble *dspec;

  if (previous_width != gimp_image_get_width (image))
    {
      dspec = G_PARAM_SPEC_DOUBLE (g_object_class_find_property (G_OBJECT_GET_CLASS (mstroke),
                                                                 "x-interval"));
      dspec->maximum = gimp_image_get_width (mstroke->image);

      dspec = G_PARAM_SPEC_DOUBLE (g_object_class_find_property (G_OBJECT_GET_CLASS (mstroke),
                                                                 "shift"));
      dspec->maximum = gimp_image_get_width (mstroke->image);
    }
  if (previous_height != gimp_image_get_height (image))
    {
      dspec = G_PARAM_SPEC_DOUBLE (g_object_class_find_property (G_OBJECT_GET_CLASS (mstroke),
                                                                 "y-interval"));
      dspec->maximum = gimp_image_get_height (mstroke->image);
    }

  if (previous_width != gimp_image_get_width (image) ||
      previous_height != gimp_image_get_height (image))
    g_signal_emit_by_name (mstroke, "update-ui", mstroke->image);
}
