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

typedef struct MBWMCompMgrClutter MBWMCompMgrClutter;
typedef struct MBWMCompMgrClutterClass MBWMCompMgrClutterClass;
typedef struct MBWMCompMgrClutterPrivate MBWMCompMgrClutterPrivate;

typedef struct MBWMCompMgrClutterClient MBWMCompMgrClutterClient;
typedef struct MBWMCompMgrClutterClientClass MBWMCompMgrClutterClientClass;
typedef struct MBWMCompMgrClutterClientPrivate MBWMCompMgrClutterClientPrivate;

typedef enum
{
  MBWMCompMgrClutterClientMapped        = (1<<0),
  MBWMCompMgrClutterClientDontUpdate    = (1<<1),
  MBWMCompMgrClutterClientDone          = (1<<2),
  MBWMCompMgrClutterClientEffectRunning = (1<<3),
  MBWMCompMgrClutterClientDontPosition  = (1<<4),
  MBWMCompMgrClutterClientDontShow      = (1<<5),
} MBWMCompMgrClutterClientFlags;

/**
 * An MBWMCompMgr which renders using Clutter.
 */
struct MBWMCompMgrClutter
{
  MBWMCompMgr                 parent;
  MBWMCompMgrClutterPrivate  *priv;
};

/**
 * Class for MBWMCompMgrClutter.
 */
struct MBWMCompMgrClutterClass
{
  MBWMCompMgrClass  parent;

  MBWMCompMgrClient * (*client_new) (MBWindowManagerClient * client);
};

int
mb_wm_comp_mgr_clutter_class_type ();

MBWMCompMgr*
mb_wm_comp_mgr_clutter_new (MBWindowManager *wm);

/**
 * An MBWMCompMgrClient rendered using Clutter.
 */
struct MBWMCompMgrClutterClient
{
  MBWMCompMgrClient               parent;

  MBWMCompMgrClutterClientPrivate *priv;
};

/**
 * Class for MBWMCompMgrClutterClient.
 */
struct MBWMCompMgrClutterClientClass
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

void
mb_wm_comp_mgr_clutter_client_track_damage (MBWMCompMgrClutterClient *cclient,
                                            Bool track_damage);

MBWMCompMgrClutterClientFlags
mb_wm_comp_mgr_clutter_client_get_flags (MBWMCompMgrClutterClient  *cclient);

MBWMList *
mb_wm_comp_mgr_clutter_get_desktops (MBWMCompMgrClutter *cmgr);

ClutterActor *
mb_wm_comp_mgr_clutter_get_nth_desktop (MBWMCompMgrClutter *cmgr, int desktop);

ClutterActor *
mb_wm_comp_mgr_clutter_get_arena (MBWMCompMgrClutter *cmgr);

#endif
