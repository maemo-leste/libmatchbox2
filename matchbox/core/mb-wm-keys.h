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

#ifndef _HAVE_MB_WM_KEYS_H
#define _HAVE_MB_WM_KEYS_H

#include <matchbox/core/mb-wm.h>

void
mb_wm_keys_binding_remove_all (MBWindowManager    *wm);

void
mb_wm_keys_binding_remove (MBWindowManager    *wm,
			   MBWMKeyBinding     *binding);
MBWMKeyBinding*
mb_wm_keys_binding_add (MBWindowManager    *wm,
			KeySym              ks,
			int                 mask,
			MBWMKeyPressedFunc  press_func,
			MBWMKeyDestroyFunc  destroy_func,
			void               *userdata);

MBWMKeyBinding*
mb_wm_keys_binding_add_with_spec (MBWindowManager    *wm,
				  const char         *keystr,
				  MBWMKeyPressedFunc  press_func,
				  MBWMKeyDestroyFunc  destroy_func,
				  void               *userdata);

void
mb_wm_keys_press (MBWindowManager *wm,
		  KeySym           keysym,
		  int              modifier_mask);

Bool
mb_wm_keys_init (MBWindowManager *wm);

#endif
