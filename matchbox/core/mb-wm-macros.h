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

#ifndef _HAVE_MB_MACROS_H
#define _HAVE_MB_MACROS_H

#if MBWM_WANT_DEBUG
#define MBWM_NOTE(type,x,a...)  do {                                  \
        if (mbwm_debug_flags & MBWM_DEBUG_##type)                     \
          { fprintf (stderr, "[" #type "] " __FILE__ ":%d,%s() " ": " x "\n", __LINE__, __func__, ##a); } \
        } while (0);

#define MBWM_MARK() MBWM_NOTE(MISC, "== mark ==")
#define MBWM_DBG(x, a...) MBWM_NOTE(MISC, x, ##a)

#include <execinfo.h>

#define _MBWM_TRACE(type,x,a...)                                  \
if (mbwm_debug_flags & MBWM_DEBUG_##type)                         \
do                                                                \
{                                                                 \
  void    *trace[10];                                             \
  size_t   depth, i;                                              \
  char   **strings;                                               \
                                                                  \
  fprintf (stderr, __FILE__ ":%d,%s(): " x "\n",                  \
	   __LINE__, __func__, ##a);                              \
                                                                  \
  depth   = backtrace (trace, sizeof(trace)/sizeof(void*));       \
  strings = backtrace_symbols (trace, depth);                     \
                                                                  \
  for (i = 1; i < depth; ++i)                                     \
    {                                                             \
      char * s = strings[i];                                      \
      while (s && *s && *s != '(')                                \
	s++;                                                      \
                                                                  \
      if (s && *s)                                                \
	s++;                                                      \
                                                                  \
      fprintf (stderr, "    %s\n", s);                            \
    }                                                             \
  free (strings);                                                 \
}while (0)

#define MBWM_TRACE() _MBWM_TRACE(TRACE, "### TRACE ###")
#define MBWM_TRACE_MSG(type,x,a...) _MBWM_TRACE(type, x, ##a)

#else /* !MBWM_ENABLE_DEBUG */

#define MBWM_NOTE(type,x,a...)
#define MBWM_DBG(x, a...)
#define MBWM_TRACE()
#define MBWM_TRACE_MSG(type,x,a...)
#define MBWM_MARK()

#endif /* MBWM_ENABLE_DEBUG */

#define MBWM_WANT_ASSERT 1

#if (MBWM_WANT_ASSERT)
#include <assert.h>
#define MBWM_ASSERT(x) assert(x)
#else
#define MBWM_ASSERT(x) do {} while (0)
#endif

/* FIXME: ifdef this with compile time flag */
#define mbwm_return_if_fail(expr) do { \
        if LIKELY(expr) { } else       \
          {                            \
	    mb_wm_util_warn (__FILE__ ":%d,%s() " ":" #expr "failed" ,\
			     __LINE__, __func__);                     \
	    return;                                                   \
          }                                                           \
	 } while(0);

#define mbwm_return_val_if_fail(expr,val) do { \
        if LIKELY(expr) { } else       \
          {                            \
	    mb_wm_util_warn (__FILE__ ":%d,%s() " ":" #expr "failed" ,\
			     __LINE__, __func__);                     \
	    return val;                                               \
          }                                                           \
	 } while(0);

#endif
