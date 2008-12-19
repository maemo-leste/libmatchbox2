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

#ifndef _HAVE_MB_WM_ROOT_WINDOW_H
#define _HAVE_MB_WM_ROOT_WINDOW_H

typedef struct MBWMRootWindow        MBWMRootWindow;
typedef struct MBWMRootWindowClass   MBWMRootWindowClass;

#define MB_WM_ROOT_WINDOW(c) ((MBWMRootWindow*)(c))
#define MB_WM_ROOT_WINDOW_CLASS(c) ((MBWMRootWindowClass*)(c))
#define MB_WM_TYPE_ROOT_WINDOW (mb_wm_root_window_class_type ())

struct MBWMRootWindow
{
  MBWMObject        parent;

  Window            xwindow;
  Window            hidden_window;
  MBWindowManager  *wm;
};

struct MBWMRootWindowClass
{
  MBWMObjectClass parent;
};

MBWMRootWindow *
mb_wm_root_window_get (MBWindowManager *wm);

int
mb_wm_root_window_class_type ();

int
mb_wm_root_window_handle_message(MBWMRootWindow *win, XClientMessageEvent *e);

void
mb_wm_root_window_update_supported_props (MBWMRootWindow *win);

#endif
