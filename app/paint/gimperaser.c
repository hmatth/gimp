/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
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

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gegl.h>

#include "paint-types.h"

#include "gegl/gimp-gegl-utils.h"

#include "core/gimp.h"
#include "core/gimpdrawable.h"
#include "core/gimpdynamics.h"
#include "core/gimpimage.h"

#include "gimpmultistroke.h"

#include "gimperaser.h"
#include "gimperaseroptions.h"

#include "gimp-intl.h"


static void   gimp_eraser_paint  (GimpPaintCore    *paint_core,
                                  GimpDrawable     *drawable,
                                  GimpPaintOptions *paint_options,
                                  GimpMultiStroke  *mstroke,
                                  GimpPaintState    paint_state,
                                  guint32           time);
static void   gimp_eraser_motion (GimpPaintCore    *paint_core,
                                  GimpDrawable     *drawable,
                                  GimpPaintOptions *paint_options,
                                  GimpMultiStroke  *mstroke);


G_DEFINE_TYPE (GimpEraser, gimp_eraser, GIMP_TYPE_BRUSH_CORE)


void
gimp_eraser_register (Gimp                      *gimp,
                      GimpPaintRegisterCallback  callback)
{
  (* callback) (gimp,
                GIMP_TYPE_ERASER,
                GIMP_TYPE_ERASER_OPTIONS,
                "gimp-eraser",
                _("Eraser"),
                "gimp-tool-eraser");
}

static void
gimp_eraser_class_init (GimpEraserClass *klass)
{
  GimpPaintCoreClass *paint_core_class = GIMP_PAINT_CORE_CLASS (klass);
  GimpBrushCoreClass *brush_core_class = GIMP_BRUSH_CORE_CLASS (klass);

  paint_core_class->paint = gimp_eraser_paint;

  brush_core_class->handles_changing_brush = TRUE;
}

static void
gimp_eraser_init (GimpEraser *eraser)
{
}

static void
gimp_eraser_paint (GimpPaintCore    *paint_core,
                   GimpDrawable     *drawable,
                   GimpPaintOptions *paint_options,
                   GimpMultiStroke  *mstroke,
                   GimpPaintState    paint_state,
                   guint32           time)
{
  switch (paint_state)
    {
    case GIMP_PAINT_STATE_MOTION:
      gimp_eraser_motion (paint_core, drawable, paint_options, mstroke);
      break;

    default:
      break;
    }
}

static void
gimp_eraser_motion (GimpPaintCore    *paint_core,
                    GimpDrawable     *drawable,
                    GimpPaintOptions *paint_options,
                    GimpMultiStroke  *mstroke)
{
  GimpEraserOptions    *options  = GIMP_ERASER_OPTIONS (paint_options);
  GimpContext          *context  = GIMP_CONTEXT (paint_options);
  GimpDynamics         *dynamics = GIMP_BRUSH_CORE (paint_core)->dynamics;
  GimpImage            *image    = gimp_item_get_image (GIMP_ITEM (drawable));
  gdouble               fade_point;
  gdouble               opacity;
  GimpLayerModeEffects  paint_mode;
  GeglBuffer           *paint_buffer;
  gint                  paint_buffer_x;
  gint                  paint_buffer_y;
  GimpRGB               background;
  GeglColor            *color;
  gdouble               force;
  const GimpCoords     *coords;
  GeglNode             *op;
  gint                  nstrokes;
  gint                  i;

  fade_point = gimp_paint_options_get_fade (paint_options, image,
                                            paint_core->pixel_dist);

  gimp_context_get_background (context, &background);
  color = gimp_gegl_color_new (&background);

  if (options->anti_erase)
    paint_mode = GIMP_ANTI_ERASE_MODE;
  else if (gimp_drawable_has_alpha (drawable))
    paint_mode = GIMP_ERASE_MODE;
  else
    paint_mode = GIMP_NORMAL_MODE;

  if (gimp_dynamics_is_output_enabled (dynamics, GIMP_DYNAMICS_OUTPUT_FORCE))
    force = gimp_dynamics_get_linear_value (dynamics,
                                            GIMP_DYNAMICS_OUTPUT_FORCE,
                                            coords,
                                            paint_options,
                                            fade_point);
  else
    force = paint_options->brush_force;

  nstrokes = gimp_multi_stroke_get_size (mstroke);

  for (i = 0; i < nstrokes; i++)
    {
      coords = gimp_multi_stroke_get_coords (mstroke, i);

      opacity = gimp_dynamics_get_linear_value (dynamics,
                                                GIMP_DYNAMICS_OUTPUT_OPACITY,
                                                coords,
                                                paint_options,
                                                fade_point);
      if (opacity == 0.0)
        continue;

      paint_buffer = gimp_paint_core_get_paint_buffer (paint_core, drawable,
                                                       paint_options, coords,
                                                       &paint_buffer_x,
                                                       &paint_buffer_y);
      if (! paint_buffer)
        continue;

      op = gimp_multi_stroke_get_operation (mstroke, paint_core,
                                            paint_buffer, i);

      gegl_buffer_set_color (paint_buffer, NULL, color);

      gimp_brush_core_paste_canvas (GIMP_BRUSH_CORE (paint_core), drawable,
                                    coords,
                                    MIN (opacity, GIMP_OPACITY_OPAQUE),
                                    gimp_context_get_opacity (context),
                                    paint_mode,
                                    gimp_paint_options_get_brush_mode (paint_options),
                                    force,
                                    paint_options->application_mode, op);
    }

  g_object_unref (color);
}
