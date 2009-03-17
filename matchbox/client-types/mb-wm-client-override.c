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

#include "mb-wm-client-override.h"

#include "mb-wm-theme.h"
#include "mb-wm-client-base.h"

static void
mb_wm_client_override_class_init (MBWMObjectClass *klass)
{
  MBWindowManagerClientClass *client;

  MBWM_MARK();

  client = (MBWindowManagerClientClass *)klass;

  /*
   * Inherit from MB_WM_CLIENT but use MB_WM_CLIENT_BASE's stack() method
   * for, well, stacking.  Because otherwise ->stacking_layer is ignored.
   * In other words poor client is not stacked at all.  Which means it is
   * left rotten at the bottom.
   */
  client->client_type  = MBWMClientTypeOverride;
  client->stack        = mb_wm_client_base_stack;

#if MBWM_WANT_DEBUG
  klass->klass_name = "MBWMClientOverride";
#endif
}

static void
mb_wm_client_override_destroy (MBWMObject *this)
{
}

static int
mb_wm_client_override_init (MBWMObject *this, va_list vap)
{
  MBWindowManagerClient *client = MB_WM_CLIENT (this);
  MBWindowManager       *wm          = client->wmref;
  MBWMClientWindow      *win         = client->window;

  if (win->xwin_transient_for
      && win->xwin_transient_for != win->xwindow
      && win->xwin_transient_for != wm->root_win->xwindow)
    {
      MBWM_DBG ("Adding to '%lx' transient list",
		win->xwin_transient_for);
      mb_wm_client_add_transient
	(mb_wm_managed_client_from_xwindow (wm,
					    win->xwin_transient_for),
	 client);
      client->stacking_layer = 0;  /* We stack with whatever transient too */
    }
  else
    {
      MBWM_DBG ("Override is transient to root or intransient");
      if (win->hildon_stacking_layer == 0)
        /* Stack with 'always on top' */
        client->stacking_layer = MBWMStackLayerTop;
      else
        client->stacking_layer = win->hildon_stacking_layer
                                 + MBWMStackLayerHildon1 - 1;
    }

  return 1;
}

int
mb_wm_client_override_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (MBWMClientOverrideClass),
	sizeof (MBWMClientOverride),
	mb_wm_client_override_init,
	mb_wm_client_override_destroy,
	mb_wm_client_override_class_init
      };

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_CLIENT, 0);
    }

  return type;
}

MBWindowManagerClient*
mb_wm_client_override_new (MBWindowManager *wm, MBWMClientWindow *win)
{
  MBWindowManagerClient *client;

  client
    = MB_WM_CLIENT(mb_wm_object_new (MB_WM_TYPE_CLIENT_OVERRIDE,
				     MBWMObjectPropWm,           wm,
				     MBWMObjectPropClientWindow, win,
				     NULL));

  return client;
}

