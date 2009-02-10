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

#ifndef _HAVE_MB_DEBUG_H
#define _HAVE_MB_DEBUG_H

/* Enumerate the fields of an MBGeometry * such that you can use it in
 * printf("%dx%d%+d%+d"). */
#define MBWM_GEOMETRY(geo) (geo)->width, (geo)->height, (geo)->x, (geo)->y

#if MBWM_WANT_DEBUG

typedef enum {
  MBWM_DEBUG_MISC            = 1 << 0,
  MBWM_DEBUG_CLIENT          = 1 << 1,
  MBWM_DEBUG_PROP            = 1 << 2,
  MBWM_DEBUG_EVENT           = 1 << 3,
  MBWM_DEBUG_PAINT           = 1 << 4,
  MBWM_DEBUG_TRACE           = 1 << 5,
  MBWM_DEBUG_OBJ_REF         = 1 << 6,
  MBWM_DEBUG_OBJ_UNREF       = 1 << 7,
  MBWM_DEBUG_OBJ             = MBWM_DEBUG_OBJ_REF | MBWM_DEBUG_OBJ_UNREF,
  MBWM_DEBUG_XAS             = 1 << 8,
  MBWM_DEBUG_COMPOSITOR      = 1 << 9,
  MBWM_DEBUG_DAMAGE          = 1 << 10,
} MBWMDebugFlag;

extern int mbwm_debug_flags;

#endif /* MBWM_WANT_DEBUG */

void
mb_wm_debug_init (const char *debug_string);

#endif
