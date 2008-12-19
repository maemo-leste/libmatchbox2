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

#ifndef _HAVE_MB_CLIENT_MENU_H
#define _HAVE_MB_CLIENT_MENU_H

#include <matchbox/core/mb-wm.h>

typedef struct MBWMClientMenu      MBWMClientMenu;
typedef struct MBWMClientMenuClass MBWMClientMenuClass;

#define MB_WM_CLIENT_MENU(c) ((MBWMClientMenu*)(c))
#define MB_WM_CLIENT_MENU_CLASS(c) ((MBWMClientMenuClass*)(c))
#define MB_WM_TYPE_CLIENT_MENU (mb_wm_client_menu_class_type ())
#define MB_WM_IS_CLIENT_MENU(c) (MB_WM_OBJECT_TYPE(c)==MB_WM_TYPE_CLIENT_MENU)

struct MBWMClientMenu
{
  MBWMClientBase    parent;
};

struct MBWMClientMenuClass
{
  MBWMClientBaseClass parent;
};

MBWindowManagerClient*
mb_wm_client_menu_new (MBWindowManager *wm, MBWMClientWindow *win);

int
mb_wm_client_menu_class_type ();

#endif
