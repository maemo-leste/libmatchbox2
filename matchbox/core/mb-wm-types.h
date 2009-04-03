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

#ifndef _HAVE_MB_WM_TYPES_H
#define _HAVE_MB_WM_TYPES_H

#include <matchbox/mb-wm-config.h>

#if USE_GLIB_MAINLOOP
#include <glib.h>
#endif

#include <X11/Xlib.h>

/**
 * Description of a callback function, as used in our homebrew signal system
 */
typedef struct MBWMFuncInfo
{
  void *func;
  void *data;
  void *userdata;
  unsigned long signal;
  unsigned long id;
} MBWMFuncInfo;

/**
 * A rectangle described by the Cartesian coordinates of its top left-hand
 * corner, its width, and its height.
 */
typedef struct MBGeometry
{
  int x,y;
  unsigned int width, height;
} MBGeometry;

typedef struct MBWMList MBWMList;

typedef void (*MBWMListForEachCB) (void *data, void *userdata);

/**
 * A simple list type (why don't we use GLib for this?)
 */
struct MBWMList
{
  MBWMList *next, *prev;
  void *data;
};

/**
 * An exact copy of XasWindowAttributes; beware code duplication; only used as
 * the return value of mb_wm_xwin_get_attributes_reply().
 */
typedef struct MBWMClientWindowAttributes /* Needs to be sorted */
{
  Visual *visual;
  Window root;
  int class;
  int bit_gravity;
  int win_gravity;
  int backing_store;
  unsigned long backing_planes;
  unsigned long backing_pixel;
  Bool save_under;
  Colormap colormap;
  Bool map_installed;
  int map_state;
  long all_event_masks;
  long your_event_mask;
  long do_not_propagate_mask;
  Bool override_redirect;

} MBWMClientWindowAttributes ;

/**
 * An icon.
 */
typedef struct MBWMRgbaIcon
{
  int width;
  int height;
  unsigned long *pixels;
} MBWMRgbaIcon;

typedef struct MBWindowManager             MBWindowManager;
typedef struct MBWindowManagerClient       MBWindowManagerClient;
typedef struct MBWindowManagerClientClass  MBWindowManagerClientClass;
typedef struct MBWindowManagerClientPriv   MBWindowManagerClientPriv;
typedef struct MBWMClientWindow            MBWMClientWindow;
typedef struct MBWMClientWindowClass       MBWMClientWindowClass;
typedef struct MBWMTheme                   MBWMTheme;
typedef struct MBWMThemeClass              MBWMThemeClass;
typedef struct MBWMThemePng                MBWMThemePng;
typedef struct MBWMThemePngClass           MBWMThemePngClass;
typedef enum   MBWMThemeCaps               MBWMThemeCaps;
typedef struct MBWMDecor                   MBWMDecor;
typedef struct MBWMDecorClass              MBWMDecorClass;
typedef struct MBWMDecorButton             MBWMDecorButton;
typedef struct MBWMDecorButtonClass        MBWMDecorButtonClass;
typedef struct MBWMLayout                  MBWMLayout;
typedef struct MBWMLayoutClass             MBWMLayoutClass;
typedef struct MBWMMainContext             MBWMMainContext;
typedef struct MBWMMainContextClass        MBWMMainContextClass;
typedef struct MBWMCompMgr                 MBWMCompMgr;
typedef struct MBWMCompMgrClass            MBWMCompMgrClass;
typedef struct MBWMCompMgrDefault          MBWMCompMgrDefault;
typedef struct MBWMCompMgrDefaultPrivate   MBWMCompMgrDefaultPrivate;
typedef struct MBWMCompMgrDefaultClass     MBWMCompMgrDefaultClass;
typedef struct MBWMCompMgrClient           MBWMCompMgrClient;
typedef struct MBWMCompMgrClientClass      MBWMCompMgrClientClass;
typedef struct MBWMCompMgrDefaultClient    MBWMCompMgrDefaultClient;
typedef struct MBWMCompMgrDefaultClientClass MBWMCompMgrDefaultClientClass;
typedef struct MBWMCompMgrDefaultClentPrivate MBWMCompMgrDefaultClientPrivate;

/**
 * The type of a MBWindowManagerClient; a particular species of X client.
 */
typedef enum MBWMClientType
{
  MBWMClientTypeApp      = (1 << 0),
  MBWMClientTypeDialog   = (1 << 1),
  MBWMClientTypePanel    = (1 << 2),
  MBWMClientTypeDesktop  = (1 << 3),
  MBWMClientTypeInput    = (1 << 4),
  MBWMClientTypeMenu     = (1 << 5),
  MBWMClientTypeNote     = (1 << 6),
  MBWMClientTypeOverride = (1 << 7),

  MBWMClientTypeLast    = MBWMClientTypeOverride,
} MBWMClientType;

typedef enum _MBWMCompMgrClientEvent
{
  MBWMCompMgrClientEventNone     = 0,
  MBWMCompMgrClientEventMinimize,
  MBWMCompMgrClientEventMap,
  MBWMCompMgrClientEventUnmap,

  _MBWMCompMgrClientEventLast,
} MBWMCompMgrClientEvent;

typedef enum _MBWMGravity
{
  MBWMGravityNone       = 0,
  MBWMGravityNorth,
  MBWMGravityNorthEast,
  MBWMGravityEast,
  MBWMGravitySouthEast,
  MBWMGravitySouth,
  MBWMGravitySouthWest,
  MBWMGravityWest,
  MBWMGravityNorthWest,
  MBWMGravityCenter,
} MBWMGravity;

typedef unsigned long MBWMCookie;

/**
 * All the static atoms we will ever need to use to communicate with
 * the X server.
 */
typedef enum MBWMAtom
{
  /* ICCCM */

  MBWM_ATOM_WM_NAME = 0,
  MBWM_ATOM_WM_STATE,
  MBWM_ATOM_WM_HINTS,
  MBWM_ATOM_WM_CHANGE_STATE,
  MBWM_ATOM_WM_PROTOCOLS,
  MBWM_ATOM_WM_DELETE_WINDOW,
  MBWM_ATOM_WM_COLORMAP_WINDOWS,
  MBWM_ATOM_WM_CLIENT_MACHINE,
  MBWM_ATOM_WM_TRANSIENT_FOR,
  MBWM_ATOM_WM_TAKE_FOCUS,

  /* EWMH */

  MBWM_ATOM_NET_WM_WINDOW_TYPE,
  MBWM_ATOM_NET_WM_WINDOW_TYPE_NORMAL,
  MBWM_ATOM_NET_WM_WINDOW_TYPE_TOOLBAR,
  MBWM_ATOM_NET_WM_WINDOW_TYPE_INPUT,
  MBWM_ATOM_NET_WM_WINDOW_TYPE_DOCK,
  MBWM_ATOM_NET_WM_WINDOW_TYPE_MENU,
  MBWM_ATOM_NET_WM_WINDOW_TYPE_POPUP_MENU,
  MBWM_ATOM_NET_WM_WINDOW_TYPE_DROPDOWN_MENU,
  MBWM_ATOM_NET_WM_WINDOW_TYPE_DIALOG,
  MBWM_ATOM_NET_WM_WINDOW_TYPE_SPLASH,
  MBWM_ATOM_NET_WM_WINDOW_TYPE_DESKTOP,
  MBWM_ATOM_NET_WM_WINDOW_TYPE_NOTIFICATION,

  MBWM_ATOM_NET_WM_STATE,
  MBWM_ATOM_NET_WM_STATE_FULLSCREEN,
  MBWM_ATOM_NET_WM_STATE_MODAL,
  MBWM_ATOM_NET_WM_STATE_ABOVE,
  MBWM_ATOM_NET_WM_STATE_STICKY,
  MBWM_ATOM_NET_WM_STATE_MAXIMIZED_VERT,
  MBWM_ATOM_NET_WM_STATE_MAXIMIZED_HORZ,
  MBWM_ATOM_NET_WM_STATE_SHADED,
  MBWM_ATOM_NET_WM_STATE_SKIP_TASKBAR,
  MBWM_ATOM_NET_WM_STATE_SKIP_PAGER,
  MBWM_ATOM_NET_WM_STATE_HIDDEN,
  MBWM_ATOM_NET_WM_STATE_BELOW,
  MBWM_ATOM_NET_WM_STATE_DEMANDS_ATTENTION,

  MBWM_ATOM_NET_SUPPORTED,
  MBWM_ATOM_NET_CLIENT_LIST,
  MBWM_ATOM_NET_NUMBER_OF_DESKTOPS,
  MBWM_ATOM_NET_ACTIVE_WINDOW,
  MBWM_ATOM_NET_SUPPORTING_WM_CHECK,
  MBWM_ATOM_NET_CLOSE_WINDOW,
  MBWM_ATOM_NET_WM_NAME,
  MBWM_ATOM_NET_WM_USER_TIME,

  MBWM_ATOM_NET_CLIENT_LIST_STACKING,
  MBWM_ATOM_NET_CURRENT_DESKTOP,
  MBWM_ATOM_NET_WM_DESKTOP,
  MBWM_ATOM_NET_WM_ICON,
  MBWM_ATOM_NET_DESKTOP_GEOMETRY,
  MBWM_ATOM_NET_WORKAREA,
  MBWM_ATOM_NET_SHOWING_DESKTOP,
  MBWM_ATOM_NET_DESKTOP_VIEWPORT,
  MBWM_ATOM_NET_FRAME_EXTENTS,
  MBWM_ATOM_NET_WM_FULL_PLACEMENT,

  MBWM_ATOM_NET_WM_ALLOWED_ACTIONS,
  MBWM_ATOM_NET_WM_ACTION_MOVE,
  MBWM_ATOM_NET_WM_ACTION_RESIZE,
  MBWM_ATOM_NET_WM_ACTION_MINIMIZE,
  MBWM_ATOM_NET_WM_ACTION_SHADE,
  MBWM_ATOM_NET_WM_ACTION_STICK,
  MBWM_ATOM_NET_WM_ACTION_MAXIMIZE_HORZ,
  MBWM_ATOM_NET_WM_ACTION_MAXIMIZE_VERT,
  MBWM_ATOM_NET_WM_ACTION_FULLSCREEN,
  MBWM_ATOM_NET_WM_ACTION_CHANGE_DESKTOP,
  MBWM_ATOM_NET_WM_ACTION_CLOSE,

  MBWM_ATOM_NET_WM_PING,
  MBWM_ATOM_NET_WM_PID,

  /* Startup Notification */
  MBWM_ATOM_NET_STARTUP_ID,

  /* Misc */

  MBWM_ATOM_UTF8_STRING,
  MBWM_ATOM_MOTIF_WM_HINTS,
  MBWM_ATOM_WIN_SUPPORTING_WM_CHECK,

  MBWM_ATOM_NET_WM_CONTEXT_HELP,
  MBWM_ATOM_NET_WM_CONTEXT_ACCEPT,
  MBWM_ATOM_NET_WM_CONTEXT_CUSTOM,
  MBWM_ATOM_NET_WM_SYNC_REQUEST,

  MBWM_ATOM_CM_TRANSLUCENCY,

  MBWM_ATOM_MB_APP_WINDOW_LIST_STACKING,
  MBWM_ATOM_MB_THEME,
  MBWM_ATOM_MB_THEME_NAME,
  MBWM_ATOM_MB_COMMAND,
  MBWM_ATOM_MB_GRAB_TRANSFER,
  MBWM_ATOM_MB_CURRENT_APP_WINDOW,
  MBWM_ATOM_MB_SECONDARY,

  /* FIXME: Custom/Unused to sort out
   *
   * MB_WM_STATE_DOCK_TITLEBAR,
   *
   * _NET_WM_SYNC_REQUEST_COUNTER,
   * _NET_WM_SYNC_REQUEST,
   *
   * MB_CLIENT_EXEC_MAP,
   * MB_CLIENT_STARTUP_LIST,
   * MB_DOCK_TITLEBAR_SHOW_ON_DESKTOP,
   *
   * WINDOW_TYPE_MESSAGE,
   *
  */

  /* special layers above others for mobile use (e.g. touchscreen lock) */
  MBWM_ATOM_HILDON_STACKING_LAYER,

  MBWM_ATOM_HILDON_WM_NAME,

  MBWM_ATOM_COUNT

} MBWMAtom;

/* Keys */

typedef struct MBWMKeyBinding MBWMKeyBinding;
typedef struct MBWMKeys       MBWMKeys;

typedef void (*MBWMKeyPressedFunc) (MBWindowManager   *wm,
				    MBWMKeyBinding    *binding,
				    void              *userdata);

typedef void (*MBWMKeyDestroyFunc) (MBWindowManager   *wm,
				    MBWMKeyBinding    *binding,
				    void              *userdata);

/**
 * A single keybinding, including callbacks for when its key is pressed and
 * for when it is destroyed.
 */
struct MBWMKeyBinding
{
  KeySym                   keysym;
  int                      modifier_mask;
  MBWMKeyPressedFunc       pressed;
  MBWMKeyDestroyFunc       destroy;
  void                    *userdata;
  /* FIXME: free func */
};

/* Event Callbacks */

typedef Bool (*MBWMXEventFunc)
     (void              *xev,
      void              *userdata);

typedef Bool (*MBWindowManagerMapNotifyFunc)
     (XMapEvent         *xev,
      void              *userdata);

typedef Bool (*MBWindowManagerClientMessageFunc)
     (XClientMessageEvent *xev,
      void                *userdata);

typedef Bool (*MBWindowManagerMapRequestFunc)
     (XMapRequestEvent  *xev,
      void              *userdata);

typedef Bool (*MBWindowManagerUnmapNotifyFunc)
     (XUnmapEvent       *xev,
      void              *userdata);

typedef Bool (*MBWindowManagerDestroyNotifyFunc)
     (XDestroyWindowEvent  *xev,
      void                 *userdata);

typedef Bool (*MBWindowManagerConfigureNotifyFunc)
     (XConfigureEvent      *xev,
      void                 *userdata);

typedef Bool (*MBWindowManagerConfigureRequestFunc)
     (XConfigureRequestEvent  *xev,
      void                    *userdata);

typedef Bool (*MBWindowManagerKeyPressFunc)
     (XKeyEvent               *xev,
      void                    *userdata);

typedef Bool (*MBWindowManagerPropertyNotifyFunc)
     (XPropertyEvent          *xev,
      void                    *userdata);

typedef Bool (*MBWindowManagerButtonPressFunc)
     (XButtonEvent            *xev,
      void                    *userdata);

typedef Bool (*MBWindowManagerButtonReleaseFunc)
     (XButtonEvent            *xev,
      void                    *userdata);

typedef Bool (*MBWindowManagerMotionNotifyFunc)
     (XMotionEvent            *xev,
      void                    *userdata);

typedef Bool (*MBWindowManagerTimeOutFunc)
     (void                    *userdata);

#if USE_GLIB_MAINLOOP
typedef GIOChannel   MBWMIOChannel;
typedef GIOCondition MBWMIOCondition;
#else
typedef int MBWMIOChannel;
typedef int MBWMIOCondition;
#endif

typedef Bool (*MBWindowManagerFdWatchFunc)
     (MBWMIOChannel           *channel,
      MBWMIOCondition          events,
      void                    *userdata);

/**
 * An association of an MBWMXEventFunc with a particular X window; for example,
 * when a window receives a ButtonPress event.
 */
typedef struct MBWMXEventFuncInfo
{
  MBWMXEventFunc func;
  Window         xwindow;
  void          *userdata;
  unsigned long  id;
}
MBWMXEventFuncInfo;

typedef struct MBWMTimeOutEventInfo MBWMTimeOutEventInfo;
typedef struct MBWMFdWatchInfo      MBWMFdWatchInfo;

typedef enum MBWMDecorButtonFlags
{
  MB_WM_DECOR_BUTTON_INVISIBLE = (1<<1),
  MB_WM_DECOR_BUTTON_NOHANDLERS = (1<<2)

} MBWMDecorButtonFlags;

typedef enum MBWMDecorType
{
  MBWMDecorTypeNorth = 1,
  MBWMDecorTypeSouth,
  MBWMDecorTypeEast,
  MBWMDecorTypeWest,

} MBWMDecorType;

typedef enum MBWMSyncType
{
  MBWMSyncStacking          = (1<<1),
  MBWMSyncGeometry          = (1<<2),
  MBWMSyncVisibility        = (1<<3),
  MBWMSyncDecor             = (1<<4),
  MBWMSyncConfigRequestAck  = (1<<5),
  MBWMSyncFullscreen        = (1<<6),
} MBWMSyncType;

/**
 * A colour, with alpha value.  May also be marked unset.
 */
typedef struct MBWMColor
{
  double r;
  double g;
  double b;
  double a;

  Bool set;
}MBWMColor;

typedef enum MBWMCompMgrShadowType
{
  MBWM_COMP_MGR_SHADOW_NONE = 0,
  MBWM_COMP_MGR_SHADOW_SIMPLE,
  MBWM_COMP_MGR_SHADOW_GAUSSIAN,
} MBWMCompMgrShadowType;

typedef enum MBWMModality
{
  MBWMModalityNormal = 0, /* Handle modality per EWMH */
  MBWMModalitySystem,     /* Treat all intransient dialogs as if system modal */
  MBWMModalityNone,       /* Ignore modality */
}MBWMModality;


/* mb remote commands */
#define MB_CMD_SET_THEME   1
#define MB_CMD_EXIT        2
#define MB_CMD_DESKTOP     3
#define MB_CMD_NEXT        4
#define MB_CMD_PREV        5
#define MB_CMD_MISC        7 	/* spare, used for debugging */
#define MB_CMD_COMPOSITE   8
#define MB_CMB_KEYS_RELOAD 9

#endif
