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

#ifndef _HAVE_MB_CLIENT_NOTE_H
#define _HAVE_MB_CLIENT_NOTE_H

#include <matchbox/core/mb-wm.h>
#include <matchbox/client-types/mb-wm-client-dialog.h>

typedef struct MBWMClientNote      MBWMClientNote;
typedef struct MBWMClientNoteClass MBWMClientNoteClass;

#define MB_WM_CLIENT_NOTE(c) ((MBWMClientNote*)(c))
#define MB_WM_CLIENT_NOTE_CLASS(c) ((MBWMClientNoteClass*)(c))
#define MB_WM_TYPE_CLIENT_NOTE (mb_wm_client_note_class_type ())
#define MB_WM_IS_CLIENT_NOTE(c) (MB_WM_OBJECT_TYPE(c)==MB_WM_TYPE_CLIENT_NOTE)

struct MBWMClientNote
{
  MBWMClientDialog  parent;
};

struct MBWMClientNoteClass
{
  MBWMClientDialogClass parent;
};

MBWindowManagerClient*
mb_wm_client_note_new (MBWindowManager *wm, MBWMClientWindow *win);

int
mb_wm_client_note_class_type ();

#endif
