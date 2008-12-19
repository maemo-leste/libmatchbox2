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

#ifndef _HAVE_MB_WM_H
#define _HAVE_MB_WM_H

#define _GNU_SOURCE 		/* For vasprintf */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>          /* for XA_ATOM etc */
#include <X11/keysym.h>         /* key mask defines */

#include <matchbox/mb-wm-config.h>
#include <matchbox/core/xas.h>    		/* async stuff not needed for xlib on xcb */

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#include <matchbox/core/mb-wm-macros.h>
#include <matchbox/core/mb-wm-debug.h>
#include <matchbox/core/mb-wm-types.h>
#include <matchbox/core/mb-wm-util.h>
#include <matchbox/core/mb-wm-object.h>
#include <matchbox/core/mb-wm-atoms.h>
#include <matchbox/core/mb-wm-props.h>
#include <matchbox/core/mb-wm-keys.h>
#include <matchbox/core/mb-wm-decor.h>
#include <matchbox/core/mb-wm-client-window.h>
#include <matchbox/core/mb-wm-root-window.h>
#include <matchbox/core/mb-wm-client.h>
#include <matchbox/core/mb-wm-client-base.h>
#include <matchbox/core/mb-wm-layout.h>
#include <matchbox/core/mb-wm-stack.h>
#include <matchbox/core/mb-window-manager.h>
#include <matchbox/core/mb-wm-main-context.h>
#endif
