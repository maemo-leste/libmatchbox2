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

#include "mb-wm-client-note.h"

#include "mb-wm-theme.h"

static void
mb_wm_client_note_class_init (MBWMObjectClass *klass)
{
  MBWindowManagerClientClass *client;

  MBWM_MARK();

  client = (MBWindowManagerClientClass *)klass;

  client->client_type  = MBWMClientTypeNote;

#if MBWM_WANT_DEBUG
  klass->klass_name = "MBWMClientNote";
#endif
}

static void
mb_wm_client_note_destroy (MBWMObject *this)
{
}

static int
mb_wm_client_note_init (MBWMObject *this, va_list vap)
{
  MBWindowManagerClient *client = MB_WM_CLIENT (this);
  MBWindowManager       *wm = client->wmref;
  MBWMClientWindow      *win = client->window;
  MBGeometry             geom;
  Bool                   change = False;
  unsigned char         *prop;
  unsigned long          n;
  Atom                   rtype;
  int                    rfmt;
  Atom actions[] = {
    wm->atoms[MBWM_ATOM_NET_WM_ACTION_CLOSE],
    wm->atoms[MBWM_ATOM_NET_WM_ACTION_MOVE],
  };

  mb_wm_util_async_trap_x_errors (wm->xdpy);
  prop = NULL;
  if (XGetWindowProperty (
      wm->xdpy, win->xwindow, wm->atoms[MBWM_ATOM_HILDON_NOTIFICATION_TYPE],
      0, -1, False,             /* offset, length, delete               */
      XA_STRING, &rtype, &rfmt, /* req_type, actual_type, actual_format */
      &n, &n,                   /* nitems, bytes_after                  */
      &prop) == Success && prop && rtype == XA_STRING && rfmt == 8)
    {
      char const *val = (char const *)prop;
      MBWMClientNote *note = MB_WM_CLIENT_NOTE (client);

      if (!strcmp (val, "_HILDON_NOTIFICATION_TYPE_BANNER"))
	note->note_type = MBWMClientNoteTypeBanner;
      else if (!strcmp (val, "_HILDON_NOTIFICATION_TYPE_INFO"))
	note->note_type = MBWMClientNoteTypeInfo;
      else if (!strcmp (val, "_HILDON_NOTIFICATION_TYPE_CONFIRMATION"))
	note->note_type = MBWMClientNoteTypeConfirmation;
      else if (!strcmp (val, "_HILDON_NOTIFICATION_TYPE_PREVIEW"))
	note->note_type = MBWMClientNoteTypeIncomingEventPreview;
      else if (!strcmp (val, "_HILDON_NOTIFICATION_TYPE_INCOMING_EVENT"))
	note->note_type = MBWMClientNoteTypeIncomingEvent;
      else
        g_critical ("0x%lx: unknown _HILDON_NOTIFICATION_TYPE", win->xwindow);
    }
  else
    g_critical ("0x%lx: _HILDON_NOTIFICATION_TYPE: fail", win->xwindow);
  if (prop)
    XFree (prop);
  mb_wm_util_async_untrap_x_errors ();

  geom = client->frame_geometry;

  if (geom.x >= wm->xdpy_width)
    {
      geom.x = wm->xdpy_width - geom.width;
      change = True;
    }

  if (geom.y >= wm->xdpy_height)
    {
      geom.y = wm->xdpy_height - geom.height;
      change = True;
    }

  switch (win->gravity)
    {
    case SouthGravity:
      geom.y = wm->xdpy_height - geom.height;
      change = True;
      break;
    case EastGravity:
      geom.x = wm->xdpy_width - geom.width;
      change = True;
      break;
    case SouthEastGravity:
      geom.y = wm->xdpy_height - geom.height;
      geom.x = wm->xdpy_width - geom.width;
      change = True;
      break;
    case NorthGravity:
      geom.y = 0;
      change = True;
      break;
    case WestGravity:
      geom.x = 0;
      change = True;
      break;
    case NorthWestGravity:
    case CenterGravity:
    case StaticGravity:
    default:
      ;
    }

  if (change)
    {
      mb_wm_client_request_geometry (client, &geom, MBWMClientReqGeomForced);
      mb_wm_client_geometry_mark_dirty (client);
    }


  XChangeProperty (wm->xdpy, win->xwindow,
		   wm->atoms[MBWM_ATOM_NET_WM_ALLOWED_ACTIONS],
		   XA_ATOM, 32, PropModeReplace,
		   (unsigned char *)actions,
		   sizeof (actions)/sizeof (actions[0]));

  return 1;
}

int
mb_wm_client_note_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (MBWMClientNoteClass),
	sizeof (MBWMClientNote),
	mb_wm_client_note_init,
	mb_wm_client_note_destroy,
	mb_wm_client_note_class_init
      };

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_CLIENT_DIALOG, 0);
    }

  return type;
}

MBWindowManagerClient*
mb_wm_client_note_new (MBWindowManager *wm, MBWMClientWindow *win)
{
  MBWindowManagerClient *client;

  client
    = MB_WM_CLIENT (mb_wm_object_new (MB_WM_TYPE_CLIENT_NOTE,
				      MBWMObjectPropWm,           wm,
				      MBWMObjectPropClientWindow, win,
				      NULL));

  return client;
}

