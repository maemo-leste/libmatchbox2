/*
 *  Matchbox Window Manager - A lightweight window manager not for the
 *                            desktop.
 *
 *  Authored By Tomas Frydrych <tf@o-hand.com>
 *
 *  Copyright (c) 2008 OpenedHand Ltd - http://o-hand.com
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

#ifndef _HAVE_MB_WM_COMP_MGR_CLUTTER_H
#define _HAVE_MB_WM_COMP_MGR_CLUTTER_H

#include <matchbox/mb-wm-config.h>
#include <clutter/clutter.h>

#define MB_WM_COMP_MGR_CLUTTER(c) ((MBWMCompMgrClutter*)(c))
#define MB_WM_COMP_MGR_CLUTTER_CLASS(c) ((MBWMCompMgrClutterClass*)(c))
#define MB_WM_TYPE_COMP_MGR_CLUTTER (mb_wm_comp_mgr_clutter_class_type ())

#define MB_WM_COMP_MGR_CLUTTER_CLIENT(c) ((MBWMCompMgrClutterClient*)(c))
#define MB_WM_COMP_MGR_CLUTTER_CLIENT_CLASS(c) ((MBWMCompMgrClutterClientClass*)(c))
#define MB_WM_TYPE_COMP_MGR_CLUTTER_CLIENT (mb_wm_comp_mgr_clutter_client_class_type ())

typedef struct _MBWMCompMgrClutter MBWMCompMgrClutter;
typedef struct _MBWMCompMgrClutterClass MBWMCompMgrClutterClass;
typedef struct _MBWMCompMgrClutterPrivate MBWMCompMgrClutterPrivate;

typedef struct _MBWMCompMgrClutterClient MBWMCompMgrClutterClient;
typedef struct _MBWMCompMgrClutterClientClass MBWMCompMgrClutterClientClass;
typedef struct _MBWMCompMgrClutterClientPrivate MBWMCompMgrClutterClientPrivate;

typedef enum
{
  MBWMCompMgrClutterClientMapped        = (1<<0),
  MBWMCompMgrClutterClientDontUpdate    = (1<<1),
  MBWMCompMgrClutterClientDone          = (1<<2),
  MBWMCompMgrClutterClientEffectRunning = (1<<3),
} MBWMCompMgrClutterClientFlags;

struct _MBWMCompMgrClutter
{
  MBWMCompMgr                 parent;
  MBWMCompMgrClutterPrivate  *priv;
};

struct _MBWMCompMgrClutterClass
{
  MBWMCompMgrClass  parent;

  MBWMCompMgrClient * (*client_new) (MBWindowManagerClient * client);
};

int
mb_wm_comp_mgr_clutter_class_type ();

MBWMCompMgr*
mb_wm_comp_mgr_clutter_new (MBWindowManager *wm);

struct _MBWMCompMgrClutterClient
{
  MBWMCompMgrClient               parent;

  MBWMCompMgrClutterClientPrivate *priv;
};

struct _MBWMCompMgrClutterClientClass
{
  MBWMCompMgrClientClass parent;
};

int
mb_wm_comp_mgr_clutter_client_class_type ();

ClutterActor *
mb_wm_comp_mgr_clutter_client_get_actor (MBWMCompMgrClutterClient *cclient);

void
mb_wm_comp_mgr_clutter_client_set_flags (MBWMCompMgrClutterClient     *cclient,
					 MBWMCompMgrClutterClientFlags flags);

void
mb_wm_comp_mgr_clutter_client_unset_flags (MBWMCompMgrClutterClient  *cclient,
					   MBWMCompMgrClutterClientFlags flags);

MBWMCompMgrClutterClientFlags
mb_wm_comp_mgr_clutter_client_get_flags (MBWMCompMgrClutterClient  *cclient);

MBWMList *
mb_wm_comp_mgr_clutter_get_desktops (MBWMCompMgrClutter *cmgr);

ClutterActor *
mb_wm_comp_mgr_clutter_get_nth_desktop (MBWMCompMgrClutter *cmgr, int desktop);

ClutterActor *
mb_wm_comp_mgr_clutter_get_arena (MBWMCompMgrClutter *cmgr);

#endif
