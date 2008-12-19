/*
 *  Matchbox Window Manager II - A lightweight window manager not for the
 *                               desktop.
 *
 *  Authored By Matthew Allum <mallum@o-hand.com>
 *
 *  Copyright (c) 2005 OpenedHand Ltd - http://o-hand.com
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#ifndef _HAVE_MB_WM_LAYOUT_MANAGER_H
#define _HAVE_MB_WM_LAYOUT_MANAGER_H

#include <matchbox/core/mb-wm.h>

#define MB_WM_LAYOUT(c) ((MBWMLayout*)(c))
#define MB_WM_LAYOUT_CLASS(c) ((MBWMLayoutClass*)(c))
#define MB_WM_TYPE_LAYOUT (mb_wm_layout_class_type ())

struct MBWMLayout
{
  MBWMObject    parent;

  MBWindowManager *wm;
};

struct MBWMLayoutClass
{
  MBWMObjectClass parent;

  void (*update) (MBWMLayout *layout);

  void (*layout_panels) (MBWMLayout *layout, MBGeometry *avail_geom);
  void (*layout_input) (MBWMLayout *layout, MBGeometry *avail_geom);
  void (*layout_free) (MBWMLayout *layout, MBGeometry *avail_geom);
  void (*layout_fullscreen) (MBWMLayout *layout, MBGeometry *avail_geom);
};

int
mb_wm_layout_class_type ();

MBWMLayout*
mb_wm_layout_new (MBWindowManager *wm);

void
mb_wm_layout_update (MBWMLayout *layout);

/* These are intended for use by subclasses of MBWMLayout */

#define SET_X      (1<<1)
#define SET_Y      (1<<2)
#define SET_WIDTH  (1<<3)
#define SET_HEIGHT (1<<4)
#define SET_ALL    (SET_X|SET_Y|SET_WIDTH|SET_HEIGHT)

Bool
mb_wm_layout_maximise_geometry (MBGeometry *geom,
				MBGeometry *max,
				int         flags);

Bool
mb_wm_layout_clip_geometry (MBGeometry *geom,
			    MBGeometry *min,
			    int         flags);

#endif
