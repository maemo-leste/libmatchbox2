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

#ifndef _HAVE_MB_CLIENT_INPUT_H
#define _HAVE_MB_CLIENT_INPUT_H

#include <matchbox/core/mb-wm.h>

typedef struct MBWMClientInput      MBWMClientInput;
typedef struct MBWMClientInputClass MBWMClientInputClass;

#define MB_WM_CLIENT_INPUT(c) ((MBWMClientInput*)(c))
#define MB_WM_CLIENT_INPUT_CLASS(c) ((MBWMClientInputClass*)(c))
#define MB_WM_TYPE_CLIENT_INPUT (mb_wm_client_input_class_type ())
#define MB_WM_IS_CLIENT_INPUT(c) (MB_WM_OBJECT_TYPE(c)==MB_WM_TYPE_CLIENT_INPUT)

/**
 * A MBWMClientBase for an input window: that is, those whose type is either
 * _NET_WM_WINDOW_TYPE_INPUT or _NET_WM_WINDOW_TYPE_TOOLBAR.
 */
struct MBWMClientInput
{
  MBWMClientBase    parent;

  MBGeometry        transient_geom;
};

/**
 * Class for MBWMClientInput.
 */
struct MBWMClientInputClass
{
  MBWMClientBaseClass parent;
};

MBWindowManagerClient*
mb_wm_client_input_new (MBWindowManager *wm, MBWMClientWindow *win);

int
mb_wm_client_input_class_type ();

#endif
