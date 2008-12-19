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

#include "mb-wm.h"

#include "mb-wm-debug.h"

#if MBWM_WANT_DEBUG
int mbwm_debug_flags = 0;

static const struct { const char *key; MBWMDebugFlag flag; } debug_keys[] = {
  { "misc",      MBWM_DEBUG_MISC },
  { "client",    MBWM_DEBUG_CLIENT },
  { "texture",   MBWM_DEBUG_PROP },
  { "event",     MBWM_DEBUG_EVENT },
  { "paint",     MBWM_DEBUG_PAINT },
  { "trace",     MBWM_DEBUG_TRACE },
  { "obj-ref",   MBWM_DEBUG_OBJ_REF },
  { "obj-unref", MBWM_DEBUG_OBJ_UNREF },
  { "xas",       MBWM_DEBUG_XAS },
  { "compositor",MBWM_DEBUG_COMPOSITOR },
  { "damage",    MBWM_DEBUG_DAMAGE },
};
#endif

void
mb_wm_debug_init (const char *debug_string)
{
#if MBWM_WANT_DEBUG
  char       *end;
  int         n, i;

  if (debug_string == NULL)
    return;

  if (streq(debug_string,"all"))
    {
      mbwm_debug_flags = 0xffff;
      return;
    }

  end = (char*)(debug_string + strlen(debug_string));

  while (debug_string < end)
    {
      n = strcspn(debug_string, ",");

      for (i=0; i<(sizeof(debug_keys)/sizeof(debug_keys[0])); i++)
	if (!strncmp(debug_string, debug_keys[i].key, n))
	  mbwm_debug_flags |= debug_keys[i].flag;

      debug_string += (n + 1);
    }
#else
  if (debug_string != NULL)
    mb_wm_util_warn ("You have requested debug messages and this matchbox "
		     "build doesn't have any!");
#endif
}

