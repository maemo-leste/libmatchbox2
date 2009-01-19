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

#ifndef _HAVE_MB_MAIN_CONTEXT_H
#define _HAVE_MB_MAIN_CONTEXT_H

#include <matchbox/core/mb-wm.h>
#include <poll.h>

#define MB_WM_MAIN_CONTEXT(c) ((MBWMMainContext*)(c))
#define MB_WM_MAIN_CONTEXT_CLASS(c) ((MBWMMainContextClass*)(c))
#define MB_WM_TYPE_MAIN_CONTEXT (mb_wm_main_context_class_type ())
#define MB_WM_IS_MAIN_CONTEXT(c) (MB_WM_OBJECT_TYPE(c)==MB_WM_TYPE_MAIN_CONTEXT)

typedef Bool (*MBWMMainContextXEventFunc) (XEvent * xev, void * userdata);

/**
 * All the handlers for the various kinds of X event.
 * \bug It might be easier to use signals and let glib do the work for us.
 */
typedef struct MBWMEventFuncs
{
  /* FIXME: figure our X wrap / unwrap mechanism */
  MBWMList *map_notify;
  MBWMList *unmap_notify;
  MBWMList *map_request;
  MBWMList *destroy_notify;
  MBWMList *configure_request;
  MBWMList *configure_notify;
  MBWMList *key_press;
  MBWMList *property_notify;
  MBWMList *button_press;
  MBWMList *button_release;
  MBWMList *motion_notify;
  MBWMList *client_message;

#if ENABLE_COMPOSITE
  MBWMList *damage_notify;
#endif

#if ! USE_GLIB_MAINLOOP
  MBWMList *timeout;
  MBWMList *fd_watch;
#endif
}
MBWMEventFuncs;

/**
 * The state of one invocation of the window manager;
 * in MBWindowManager; contains the MBWMEventFuncs.
 */
struct MBWMMainContext
{
  MBWMObject       parent;

  /** The current window manager */
  MBWindowManager *wm;

  /** All the X event handlers */
  MBWMEventFuncs   event_funcs;
  struct pollfd   *poll_fds;
  int              n_poll_fds;
  Bool             poll_cache_dirty;
};

/**
 * Class for MBWMMainContext.
 */
struct MBWMMainContextClass
{
  MBWMObjectClass parent;
};

int
mb_wm_main_context_class_type ();

MBWMMainContext*
mb_wm_main_context_new(MBWindowManager *wm);

/** Adds a new X event handler */
unsigned long
mb_wm_main_context_x_event_handler_add (MBWMMainContext *ctx,
					Window           xwin,
					int              type,
					MBWMXEventFunc   func,
					void            *userdata);

/** Removes an existing X event handler */
void
mb_wm_main_context_x_event_handler_remove (MBWMMainContext *ctx,
					   int              type,
					   unsigned long    id);

unsigned long
mb_wm_main_context_timeout_handler_add (MBWMMainContext            *ctx,
					int                         ms,
					MBWindowManagerTimeOutFunc  func,
					void                       *userdata);

void
mb_wm_main_context_timeout_handler_remove (MBWMMainContext *ctx,
					   unsigned long    id);

MBWMIOChannel *
mb_wm_main_context_io_channel_new (int fd);

void
mb_wm_main_context_io_channel_destroy (MBWMIOChannel * channel);

int
mb_wm_main_context_io_channel_get_fd (MBWMIOChannel * channel);

unsigned long
mb_wm_main_context_fd_watch_add (MBWMMainContext           *ctx,
				 MBWMIOChannel             *channel,
				 MBWMIOCondition            events,
				 MBWindowManagerFdWatchFunc func,
				 void                      *userdata);

void
mb_wm_main_context_fd_watch_remove (MBWMMainContext *ctx,
				    unsigned long    id);

#if USE_GLIB_MAINLOOP
gboolean
mb_wm_main_context_gloop_xevent (gpointer userdata);
#endif

Bool
mb_wm_main_context_handle_x_event (XEvent          *xev,
				   MBWMMainContext *ctx);

void
mb_wm_main_context_loop (MBWMMainContext *ctx);

Bool
mb_wm_main_context_spin_loop (MBWMMainContext *ctx);

#endif
