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

/**
 * The root window; don't confuse this with MBWMClientDesktop,
 * which represents the desktop window.
 */
struct MBWMRootWindow
{
  MBWMObject        parent;

  /** ID of the root window */
  Window            xwindow;

  /** ID of the magic offscreen window that shows we're EWMH-compatible */
  Window            hidden_window;

  /** The window manager */
  MBWindowManager  *wm;
};

/**
 * Class for MBWMRootWindow.
 */
struct MBWMRootWindowClass
{
  MBWMObjectClass parent;
};

/**
 * Finds the one and only root window of a window manager.
 */
MBWMRootWindow *
mb_wm_root_window_get (MBWindowManager *wm);

int
mb_wm_root_window_class_type ();

/**
 * Handles a message sent to the root window.  Returns true iff the message
 * was handled.
 */
int
mb_wm_root_window_handle_message(MBWMRootWindow *win, XClientMessageEvent *e);

/**
 * Marks the root window with details of which properties this window
 * manager can handle.
 */
void
mb_wm_root_window_update_supported_props (MBWMRootWindow *win);

#endif
