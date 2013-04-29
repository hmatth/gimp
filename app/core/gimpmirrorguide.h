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

#ifndef __GIMP_MIRROR_GUIDE_H__
#define __GIMP_MIRROR_GUIDE_H__


#include "gimpguide.h"


#define GIMP_TYPE_MIRROR_GUIDE            (gimp_mirror_guide_get_type ())
#define GIMP_MIRROR_GUIDE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_MIRROR_GUIDE, GimpMirrorGuide))
#define GIMP_MIRROR_GUIDE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_MIRROR_GUIDE, GimpMirrorGuideClass))
#define GIMP_IS_MIRROR_GUIDE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_MIRROR_GUIDE))
#define GIMP_IS_MIRROR_GUIDE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_MIRROR_GUIDE))
#define GIMP_MIRROR_GUIDE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_MIRROR_GUIDE, GimpMirrorGuideClass))


typedef struct _GimpMirrorGuideClass GimpMirrorGuideClass;

struct _GimpMirrorGuide
{
  GimpGuide         parent_instance;

  Gimp             *gimp;
};

struct _GimpMirrorGuideClass
{
  GimpGuideClass    parent_class;

  /*  signals  */
  void (* removed)  (GimpMirrorGuide  *mirror_guide);
};


GType               gimp_mirror_guide_get_type        (void) G_GNUC_CONST;

GimpMirrorGuide *   gimp_mirror_guide_new             (Gimp                *gimp,
                                                       GimpOrientationType  orientation,
                                                       guint32              mirror_guide_ID);

#endif /* __GIMP_MIRROR_GUIDE_H__ */
