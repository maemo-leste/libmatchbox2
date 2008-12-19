/*
 *  Matchbox Window Manager - A lightweight window manager not for the
 *                            desktop.
 *
 *  Authored By Matthew Allum <mallum@o-hand.com>
 *              Tomas Frydrych <tf@o-hand.com>
 *
 *  Copyright (c) 2002, 2004, 2007 OpenedHand Ltd - http://o-hand.com
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

#ifndef _HAVE_MB_WM_COMP_MGR_DEFAULT_H
#define _HAVE_MB_WM_COMP_MGR_DEFAULT_H

#include <matchbox/mb-wm-config.h>

#define MB_WM_COMP_MGR_DEFAULT(c) ((MBWMCompMgrDefault*)(c))
#define MB_WM_COMP_MGR_DEFAULT_CLASS(c) ((MBWMCompMgrDefaultClass*)(c))
#define MB_WM_TYPE_COMP_MGR_DEFAULT (mb_wm_comp_mgr_xrender_class_type ())

#define MB_WM_COMP_MGR_DEFAULT_CLIENT(c) ((MBWMCompMgrDefaultClient*)(c))
#define MB_WM_COMP_MGR_DEFAULT_CLIENT_CLASS(c) ((MBWMCompMgrDefaultClientClass*)(c))
#define MB_WM_TYPE_COMP_MGR_DEFAULT_CLIENT (mb_wm_comp_mgr_xrender_client_class_type ())

struct MBWMCompMgrDefault
{
  MBWMCompMgr                 parent;
  MBWMCompMgrDefaultPrivate  *priv;
};

struct MBWMCompMgrDefaultClass
{
  MBWMCompMgrClass parent;
};

int
mb_wm_comp_mgr_xrender_class_type ();

MBWMCompMgr*
mb_wm_comp_mgr_xrender_new (MBWindowManager *wm);

struct MBWMCompMgrDefaultClientClass
{
  MBWMCompMgrClientClass  parent;
};

int
mb_wm_comp_mgr_xrender_client_class_type ();

#endif
