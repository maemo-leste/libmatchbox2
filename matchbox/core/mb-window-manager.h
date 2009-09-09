/*
 *  Matchbox Window Manager II - A lightweight window manager not for the
 *                               desktop.
 *
 *  Authored By Matthew Allum <mallum@o-hand.com>
 *
 *  Copyright (c) 2005, 2007 OpenedHand Ltd - http://o-hand.com
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

#ifndef _HAVE_MB_WM_WINDOW_MANAGER_H
#define _HAVE_MB_WM_WINDOW_MANAGER_H

/**
 * \mainpage
 *
 * If you're looking for a quick overview, the best place to start
 * reading is the MBWindowManager class.  MBWindowManager contains
 * some MBWindowManagerClient objects, each of which represents a
 * window of a particular type; there is a subclass for each of these
 * types (such as MBWMClientDialog).  Each MBWindowManagerClient contains
 * an MBWMClientWindow which represents the actual low-level X window.

 * MBWindowManager also contains an MBWMTheme, which is usually in
 * practice an instance of the subclass MBWMThemePng.
 *
 * MBWindowManager also contains an MBWMCompMgr, if this was enabled
 * during configuration, which is either an MBWMCompMgrDefault
 * (to use XRender) or an MBWMCompMgrClutter (to use Clutter).
 *
 * MBWindowManager also contains an MBWMMainContext, which contains an
 * MBWMEventFuncs.  This is a homebrew signal system which can call any
 * number of callback functions when a given X event occurs.
 * The window manager also has its own object system which resembles GObject.
 * Its root object is MBWMObject.
 */

#include <matchbox/mb-wm-config.h>
#include <matchbox/core/mb-wm-object.h>
#include <matchbox/core/mb-wm-root-window.h>
#include <matchbox/core/xas.h>

typedef struct MBWindowManagerClass   MBWindowManagerClass;
typedef struct MBWindowManagerPriv    MBWindowManagerPriv;

#define MB_WINDOW_MANAGER(c)       ((MBWindowManager*)(c))
#define MB_WINDOW_MANAGER_CLASS(c) ((MBWindowManagerClass*)(c))
#define MB_TYPE_WINDOW_MANAGER     (mb_wm_class_type ())

typedef enum MBWindowManagerFlag
{
  MBWindowManagerFlagDesktop           = (1<<0),
  MBWindowManagerFlagAlwaysReloadTheme = (1<<1),
} MBWindowManagerFlag;

typedef enum
{
  MBWindowManagerSignalThemeChange = 1,
  MBWindowManagerSignalRootConfigure, 
} MBWindowManagerSignal;

typedef enum
{
  MBWindowManagerCursorNone = 0,
  MBWindowManagerCursorLeftPtr,

  _MBWindowManagerCursorLast
} MBWindowManagerCursor;

/**
 * The general, overall state of this window manager, containing some
 * MBWindowManagerClient objects, and the MBWMTheme, MBWMRootWindow,
 * MBWMLayout, MBWMKeys, MBWMMainContext, and MBWMCompMgr if any.
 */
struct MBWindowManager
{
  MBWMObject                   parent;

  Display                     *xdpy;
  unsigned int                 xdpy_width, xdpy_height;
  int                          xscreen;

  MBWindowManagerClient       *stack_top, *stack_bottom;
  MBWMList                    *clients;
  MBWindowManagerClient       *desktop;
  MBWindowManagerClient       *focused_client;

  int                          n_desktops;
  int                          active_desktop;

  Atom                         atoms[MBWM_ATOM_COUNT];

  MBWMKeys                    *keys; /* Keybindings etc */

  XasContext                  *xas_context;

  /* ### Private ### */
  MBWMSyncType                 sync_type;
  int                          client_type_cnt;
  int                          stack_n_clients;
  MBWMRootWindow              *root_win;

  const char                  *sm_client_id;

  MBWMTheme                   *theme;
  MBWMLayout                  *layout;
  MBWMMainContext             *main_ctx;
  MBWindowManagerFlag          flags;
#if ENABLE_COMPOSITE
  MBWMCompMgr                 *comp_mgr;
  int                          damage_event_base;
#endif

  MBWindowManagerCursor        cursor;
  Cursor                       cursors[_MBWindowManagerCursorLast];

  /* Temporary stuff, only valid during object initialization */
  const char                  *theme_path;

  MBWMModality                 modality_type;

  char                       **argv;
  int                          argc;
  Bool                         non_redirection;
};

/**
 * Class of MBWindowManager.
 */
struct MBWindowManagerClass
{
  MBWMObjectClass parent;

  void (*process_cmdline) (MBWindowManager * wm);

  MBWindowManagerClient* (*client_new) (MBWindowManager *wm,
					MBWMClientWindow *w);
  MBWMLayout           * (*layout_new) (MBWindowManager *wm);

  /* These return True if now further action to be taken */
  Bool (*client_activate)   (MBWindowManager *wm, MBWindowManagerClient *c);
  void (*client_responding) (MBWindowManager *wm, MBWindowManagerClient *c);
  Bool (*client_hang)       (MBWindowManager *wm, MBWindowManagerClient *c);

  MBWMTheme * (*theme_new)  (MBWindowManager *wm, const char * path);

#if ENABLE_COMPOSITE
  MBWMCompMgr * (*comp_mgr_new) (MBWindowManager *wm);
#endif

  void (*main) (MBWindowManager *wm);
};

MBWindowManager *
mb_wm_new (int argc, char **argv);

MBWindowManager *
mb_wm_new_with_dpy (int argc, char **argv, Display * dpy);

void
mb_wm_init (MBWindowManager * wm);

int
mb_wm_class_type ();

void
mb_wm_main_loop(MBWindowManager *wm);

MBWindowManagerClient*
mb_wm_managed_client_from_xwindow(MBWindowManager *wm, Window win);

MBWindowManagerClient*
mb_wm_managed_client_from_frame (MBWindowManager *wm, Window frame);

int
mb_wm_register_client_type (void);

void
mb_wm_display_sync_queue (MBWindowManager* wm, MBWMSyncType sync);

void
mb_wm_get_display_geometry (MBWindowManager  *wm,
			    MBGeometry       *geometry);

void
mb_wm_activate_client(MBWindowManager * wm, MBWindowManagerClient *c);

void __attribute__ ((visibility("hidden")))
mb_wm_handle_ping_reply (MBWindowManager * wm, MBWindowManagerClient *c);

void __attribute__ ((visibility("hidden")))
mb_wm_handle_hang_client (MBWindowManager * wm, MBWindowManagerClient *c);

void
mb_wm_handle_show_desktop (MBWindowManager * wm, Bool show);

void
mb_wm_toggle_desktop (MBWindowManager * wm);

MBWindowManagerClient*
mb_wm_get_visible_main_client(MBWindowManager *wm);

void
mb_wm_unfocus_client (MBWindowManager *wm, MBWindowManagerClient *client);

void
mb_wm_cycle_apps (MBWindowManager *wm, Bool reverse);

void
mb_wm_set_theme_from_path (MBWindowManager *wm, const char *theme_path);

void __attribute__ ((visibility("hidden")))
mb_wm_compositing_on (MBWindowManager * wm);

void __attribute__ ((visibility("hidden")))
mb_wm_compositing_off (MBWindowManager * wm);

Bool __attribute__ ((visibility("hidden")))
mb_wm_compositing_enabled (MBWindowManager * wm);

MBWMModality
mb_wm_get_modality_type (MBWindowManager * wm);

void
mb_wm_sync (MBWindowManager *wm);

void __attribute__ ((visibility("hidden")))
mb_wm_select_desktop (MBWindowManager *wm, int desktop);

void
mb_adjust_dialog_title_position (MBWindowManager *wm,
                                 int new_padding);
void
mb_wm_setup_redirection (MBWindowManager *wm, int redirection);

#endif
