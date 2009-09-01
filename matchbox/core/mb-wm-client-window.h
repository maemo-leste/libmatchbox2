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

#ifndef _HAVE_MB_WM_CLIENT_WINDOW_H
#define _HAVE_MB_WM_CLIENT_WINDOW_H

/* FIXME: below limits to 32 props */

/* When a property changes
 *    - window updates its internal values
 *    - somehow signals client object to process with what changed.
 */

#define MBWM_WINDOW_PROP_WIN_TYPE        (1<<0)
#define MBWM_WINDOW_PROP_GEOMETRY        (1<<1)
#define MBWM_WINDOW_PROP_ATTR            (1<<2)
#define MBWM_WINDOW_PROP_NAME            (1<<3)
#define MBWM_WINDOW_PROP_SIZE_HINTS      (1<<4)
#define MBWM_WINDOW_PROP_WM_HINTS        (1<<5)
#define MBWM_WINDOW_PROP_NET_ICON        (1<<6)
#define MBWM_WINDOW_PROP_NET_PID         (1<<7)
#define MBWM_WINDOW_PROP_PROTOS          (1<<8)
#define MBWM_WINDOW_PROP_TRANSIENCY      (1<<9)
#define MBWM_WINDOW_PROP_STATE           (1<<10)
#define MBWM_WINDOW_PROP_NET_STATE       (1<<11)
#define MBWM_WINDOW_PROP_STARTUP_ID      (1<<12)
#define MBWM_WINDOW_PROP_CLIENT_MACHINE  (1<<13)
#define MBWM_WINDOW_PROP_ALLOWED_ACTIONS (1<<14)
#define MBWM_WINDOW_PROP_NET_USER_TIME   (1<<15)
#define MBWM_WINDOW_PROP_CM_TRANSLUCENCY (1<<17)
#define MBWM_WINDOW_PROP_MWM_HINTS       (1<<18)
#define MBWM_WINDOW_PROP_HILDON_STACKING (1<<19)
#define MBWM_WINDOW_PROP_WIN_HILDON_TYPE (1<<20)

#define MBWM_WINDOW_PROP_ALL        (0xffffffff)

typedef enum MBWMClientWindowEWMHState
  {
    MBWMClientWindowEWMHStateModal            = (1<<0),
    MBWMClientWindowEWMHStateSticky           = (1<<1),
    MBWMClientWindowEWMHStateMaximisedVert    = (1<<2),
    MBWMClientWindowEWMHStateMaximisedHorz    = (1<<3),
    MBWMClientWindowEWMHStateShaded           = (1<<4),
    MBWMClientWindowEWMHStateSkipTaskbar      = (1<<5),
    MBWMClientWindowEWMHStateSkipPager        = (1<<6),
    MBWMClientWindowEWMHStateHidden           = (1<<7),
    MBWMClientWindowEWMHStateFullscreen       = (1<<8),
    MBWMClientWindowEWMHStateAbove            = (1<<9),
    MBWMClientWindowEWMHStateBelow            = (1<<10),
    MBWMClientWindowEWMHStateDemandsAttention = (1<<11),
    /*
     * Keep in sync with the MBWMClientWindowEWHMStatesCount define below !!!
     */
  }
MBWMClientWindowEWMHState;

#define MBWMClientWindowEWHMStatesCount 12

typedef enum MBWMClientWindowStateChange
  {
    MBWMClientWindowStateChangeRemove  = (1<<0),
    MBWMClientWindowStateChangeAdd     = (1<<1),
    MBWMClientWindowStateChangeToggle  = (1<<2),
  }
MBWMClientWindowStateChange;

typedef enum MBWMClientWindowProtos
  {
    MBWMClientWindowProtosFocus         = (1<<0),
    MBWMClientWindowProtosDelete        = (1<<1),
    MBWMClientWindowProtosContextHelp   = (1<<2),
    MBWMClientWindowProtosContextAccept = (1<<3),
    MBWMClientWindowProtosContextCustom = (1<<4),
    MBWMClientWindowProtosPing          = (1<<5),
    MBWMClientWindowProtosSyncRequest   = (1<<6),
  }
MBWMClientWindowProtos;

typedef enum MBWMClientWindowAllowedActions
  {
    MBWMClientWindowActionMove          = (1<<0),
    MBWMClientWindowActionResize        = (1<<1),
    MBWMClientWindowActionMinimize      = (1<<2),
    MBWMClientWindowActionShade         = (1<<3),
    MBWMClientWindowActionStick         = (1<<4),
    MBWMClientWindowActionMaximizeHorz  = (1<<5),
    MBWMClientWindowActionMaximizeVert  = (1<<6),
    MBWMClientWindowActionFullscreen    = (1<<7),
    MBWMClientWindowActionChangeDesktop = (1<<8),
    MBWMClientWindowActionClose         = (1<<9),
  }
MBWMClientWindowAllowedActions;

#define MB_WM_CLIENT_WINDOW(c) ((MBWMClientWindow*)(c))
#define MB_WM_CLIENT_WINDOW_CLASS(c) ((MBWMClientWindowClass*)(c))
#define MB_WM_TYPE_CLIENT_WINDOW (mb_wm_client_window_class_type ())

/**
 * Any toplevel window on the X server; not necessarily of any
 * type that we recognise; compare MBWindowManagerClient, which
 * represents only types we recognise (and contains this class).
 */
struct MBWMClientWindow
{
  MBWMObject    parent;

  MBGeometry                     geometry;
  MBGeometry                     x_geometry;
  unsigned int                   depth;
  char                          *name;
  Bool                           name_has_markup;
  Window                         xwindow;
  Visual                        *visual;
  Colormap                       colormap;
  MBWindowManager               *wm;

  Atom                           net_type;
  Atom                           hildon_type;
  Bool                           want_key_input;
  Window                         xwin_group;
  Pixmap                         icon_pixmap, icon_pixmap_mask;

  /** WithdrawnState 0, NormalState 1, IconicState 3 */
  int                            initial_state ;

  MBWMClientWindowEWMHState      ewmh_state;
  Window                         xwin_transient_for;

  MBWMClientWindowProtos         protos;
  pid_t                          pid;
  int                            translucency;
  char                          *machine;

  MBWMList                      *icons;

  MBWMClientWindowAllowedActions allowed_actions;

  unsigned long                  user_time;

  int                            gravity;
  int                            window_class;
  Bool                           override_redirect;
  Bool                           undecorated;

  /* value of the atom _HILDON_STACKING_LAYER (1-10) */
  unsigned int                   hildon_stacking_layer;
};

struct MBWMClientWindowClass
{
  MBWMObjectClass parent;
};

int
mb_wm_client_window_class_type ();

MBWMClientWindow*
mb_wm_client_window_new (MBWindowManager *wm, Window xwin);

Bool
mb_wm_client_window_sync_properties (MBWMClientWindow *win,
				     unsigned long     props_req);

Bool
mb_wm_client_window_is_state_set (MBWMClientWindow *win,
				  MBWMClientWindowEWMHState state);

#endif
