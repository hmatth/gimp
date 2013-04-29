/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * GimpMirrorGuide
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

#include "core-types.h"

#include "core/gimptoolinfo.h"

#include "paint/gimppaintoptions.h"

#include "gimp.h"
#include "gimpmirrorguide.h"

enum
{
  PROP_0,
  PROP_GIMP
};

static void   gimp_mirror_guide_finalize     (GObject      *object);
static void   gimp_mirror_guide_get_property (GObject      *object,
                                              guint         property_id,
                                              GValue       *value,
                                              GParamSpec   *pspec);
static void   gimp_mirror_guide_set_property (GObject      *object,
                                              guint         property_id,
                                              const GValue *value,
                                              GParamSpec   *pspec);

G_DEFINE_TYPE (GimpMirrorGuide, gimp_mirror_guide, GIMP_TYPE_GUIDE)

#define parent_class gimp_mirror_guide_parent_class

static void
gimp_mirror_guide_class_init (GimpMirrorGuideClass *klass)
{
  GObjectClass *object_class  = G_OBJECT_CLASS (klass);

  object_class->get_property  = gimp_mirror_guide_get_property;
  object_class->set_property  = gimp_mirror_guide_set_property;

  object_class->finalize      = gimp_mirror_guide_finalize;

  g_object_class_install_property (object_class, PROP_GIMP,
                                   g_param_spec_object ("gimp",
                                                        NULL, NULL,
                                                        GIMP_TYPE_GIMP,
                                                        GIMP_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY));
}

static void
gimp_mirror_guide_init (GimpMirrorGuide *mirror_guide)
{
}

static void
gimp_mirror_guide_finalize (GObject *object)
{
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gimp_mirror_guide_get_property (GObject      *object,
                                guint         property_id,
                                GValue       *value,
                                GParamSpec   *pspec)
{
  GimpMirrorGuide *guide = GIMP_MIRROR_GUIDE (object);

  switch (property_id)
    {
    case PROP_GIMP:
      g_value_set_object (value, guide->gimp);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_mirror_guide_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GimpMirrorGuide *guide = GIMP_MIRROR_GUIDE (object);

  switch (property_id)
    {
    case PROP_GIMP:
      guide->gimp = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}
GimpMirrorGuide *
gimp_mirror_guide_new (Gimp                *gimp,
                       GimpOrientationType  orientation,
                       guint32              guide_ID)
{
  return g_object_new (GIMP_TYPE_MIRROR_GUIDE,
                       "id",          guide_ID,
                       "orientation", orientation,
                       "gimp",        gimp,
                       NULL);
}
