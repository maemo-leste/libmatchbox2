/*
 *  Matchbox Window Manager II - A lightweight window manager not for the
 *                               desktop.
 *
 *  Authored By Tomas Frydrych <tf@o-hand.com>
 *
 *  Copyright (c) 2007 OpenedHand Ltd - http://o-hand.com
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

#ifndef _HAVE_MB_CLIENT_OVERRIDE_H
#define _HAVE_MB_CLIENT_OVERRIDE_H

#include <matchbox/core/mb-wm.h>

typedef struct MBWMClientOverride      MBWMClientOverride;
typedef struct MBWMClientOverrideClass MBWMClientOverrideClass;

#define MB_WM_CLIENT_OVERRIDE(c) ((MBWMClientOverride*)(c))
#define MB_WM_CLIENT_OVERRIDE_CLASS(c) ((MBWMClientOverrideClass*)(c))
#define MB_WM_TYPE_CLIENT_OVERRIDE (mb_wm_client_override_class_type ())
#define MB_WM_IS_CLIENT_OVERRIDE(c) (MB_WM_OBJECT_TYPE(c)==MB_WM_TYPE_CLIENT_OVERRIDE)

/**
 * A MBWMClientBase for override-redirect windows: that is, those which have the
 * X window attribute "override-redirect"; the window manager must not attempt
 * to decorate these windows.
 */
struct MBWMClientOverride
{
  MBWindowManagerClient    parent;
};

/**
 * Class for MBWMClientOverride.
 */
struct MBWMClientOverrideClass
{
  MBWindowManagerClientClass parent;
};

MBWindowManagerClient*
mb_wm_client_override_new (MBWindowManager *wm, MBWMClientWindow *win);

int
mb_wm_client_override_class_type ();

#endif
