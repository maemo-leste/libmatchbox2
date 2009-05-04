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

#ifndef _HAVE_MB_CLIENT_DIALOG_H
#define _HAVE_MB_CLIENT_DIALOG_H

#include <matchbox/core/mb-wm.h>

typedef struct MBWMClientDialog      MBWMClientDialog;
typedef struct MBWMClientDialogClass MBWMClientDialogClass;

#define MB_WM_CLIENT_DIALOG(c) ((MBWMClientDialog*)(c))
#define MB_WM_CLIENT_DIALOG_CLASS(c) ((MBWMClientDialogClass*)(c))
#define MB_WM_TYPE_CLIENT_DIALOG (mb_wm_client_dialog_class_type ())
#define MB_WM_IS_CLIENT_DIALOG(c) (MB_WM_OBJECT_TYPE(c)==MB_WM_TYPE_CLIENT_DIALOG)
#define MB_WM_PARENT_IS_CLIENT_DIALOG(c) (MB_WM_OBJECT_PARENT_TYPE(c)==MB_WM_TYPE_CLIENT_DIALOG)

/**
 * A MBWMClientBase for dialogue windows: that is, those whose type is
 * _NET_WM_WINDOW_TYPE_DIALOG.
 */
struct MBWMClientDialog
{
  MBWMClientBase    parent;
  MBWMDecorButton  *button_close;
};

/**
 * Class for MBWMClientDialog.
 */
struct MBWMClientDialogClass
{
  MBWMClientBaseClass parent;
};

MBWindowManagerClient*
mb_wm_client_dialog_new (MBWindowManager *wm, MBWMClientWindow *win);

int
mb_wm_client_dialog_class_type ();

#endif
