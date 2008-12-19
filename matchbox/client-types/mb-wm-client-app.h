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

#ifndef _HAVE_MB_CLIENT_APP_H
#define _HAVE_MB_CLIENT_APP_H

#include <matchbox/core/mb-wm.h>

typedef struct MBWMClientApp      MBWMClientApp;
typedef struct MBWMClientAppClass MBWMClientAppClass;

#define MB_WM_CLIENT_APP(c) ((MBWMClientApp*)(c))
#define MB_WM_CLIENT_APP_CLASS(c) ((MBWMClientAppClass*)(c))
#define MB_WM_TYPE_CLIENT_APP (mb_wm_client_app_class_type ())
#define MB_WM_IS_CLIENT_APP(c) (MB_WM_OBJECT_TYPE(c)==MB_WM_TYPE_CLIENT_APP)

struct MBWMClientApp
{
  MBWMClientBase    parent;
};

struct MBWMClientAppClass
{
  MBWMClientBaseClass parent;

};

MBWindowManagerClient*
mb_wm_client_app_new(MBWindowManager *wm, MBWMClientWindow *win);

int
mb_wm_client_app_class_type ();

#endif
