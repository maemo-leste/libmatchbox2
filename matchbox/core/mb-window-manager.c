/*
 *  Matchbox Window Manager II - A lightweight window manager not for the
 *                               desktop.
 *
 *  Authored By Matthew Allum <mallum@o-hand.com>
 *              Tomas Frydrych <tf@o-hand.com>
 *
 *  Copyright (c) 2005, 2007, 2008 OpenedHand Ltd - http://o-hand.com
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
#include "../client-types/mb-wm-client-app.h"
#include "../client-types/mb-wm-client-panel.h"
#include "../client-types/mb-wm-client-dialog.h"
#include "../client-types/mb-wm-client-desktop.h"
#include "../client-types/mb-wm-client-input.h"
#include "../client-types/mb-wm-client-note.h"
#include "../client-types/mb-wm-client-menu.h"
#include "../theme-engines/mb-wm-theme.h"

#if ENABLE_COMPOSITE
# include "mb-wm-comp-mgr.h"
#  if ENABLE_CLUTTER_COMPOSITE_MANAGER
#   include <clutter/x11/clutter-x11.h>
#   include "mb-wm-comp-mgr-clutter.h"
#  else
#   include "mb-wm-comp-mgr-xrender.h"
#  endif
# include "../client-types/mb-wm-client-override.h"
# include <X11/extensions/Xdamage.h>
# include <X11/extensions/Xrender.h>
# include <X11/extensions/Xcomposite.h>
#endif

#if USE_GTK
#  include <gdk/gdk.h>
#endif

#include <stdarg.h>

#include <X11/Xmd.h>
#include <X11/XKBlib.h>

#ifdef HAVE_XFIXES
#include <X11/extensions/Xfixes.h> /* Used to hide the cursor */
#endif

#define FALLBACK_THEME_PATH "/usr/share/themes/default"

static void
mb_wm_process_cmdline (MBWindowManager *wm);

static void
mb_wm_focus_client (MBWindowManager *wm, MBWindowManagerClient *client);

static Bool
mb_wm_activate_client_real (MBWindowManager * wm, MBWindowManagerClient *c);

static void
mb_wm_update_root_win_rectangles (MBWindowManager *wm);

static  Bool
mb_wm_handle_root_config_notify (XConfigureEvent *xev,
			    void            *userdata);

static Bool
mb_wm_is_my_window (MBWindowManager *wm, Window xwin,
		    MBWindowManagerClient **client);

static void
mb_wm_unmanage_client (MBWindowManager       *wm,
		       MBWindowManagerClient *client,
		       Bool                   destroy);

static void
mb_wm_manage_client (MBWindowManager       *wm,
		     MBWindowManagerClient *client,
		     Bool                   activate);

static void
mb_wm_set_layout (MBWindowManager *wm, MBWMLayout *layout);

static void
mb_wm_set_n_desktops (MBWindowManager *wm, int n_desktops);

static void
mb_wm_set_theme (MBWindowManager *wm, MBWMTheme * theme);

static MBWindowManagerClient*
mb_wm_client_new_func (MBWindowManager *wm, MBWMClientWindow *win)
{
  if (win->override_redirect)
    {
      MBWM_DBG ("### override-redirect window ###\n");
#if ENABLE_COMPOSITE
      return mb_wm_client_override_new (wm, win);
#else
      return NULL;
#endif
    }

  if (win->net_type == wm->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_DOCK])
    {
      MBWM_DBG ("### is panel ###\n");
      return mb_wm_client_panel_new(wm, win);
    }
  else if (win->net_type == wm->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_DIALOG])
    {
      MBWM_DBG ("### is dialog ###\n");
      return mb_wm_client_dialog_new(wm, win);
    }
  else if (win->net_type == wm->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_NOTIFICATION])
    {
      MBWM_DBG ("### is notification ###\n");
      return mb_wm_client_note_new (wm, win);
    }
  else if (win->net_type ==wm->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_MENU] ||
	   win->net_type ==wm->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_POPUP_MENU]||
	   win->net_type ==wm->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_DROPDOWN_MENU])
    {
      MBWM_DBG ("### is menu ###\n");
      return mb_wm_client_menu_new (wm, win);
    }
  else if (win->net_type == wm->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_DESKTOP])
    {
      MBWM_DBG ("### is desktop ###\n");
      /* Only one desktop allowed */
      if (wm->desktop)
	return NULL;

      return mb_wm_client_desktop_new (wm, win);
    }
  else if (win->net_type == wm->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_TOOLBAR] ||
	   win->net_type == wm->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_INPUT])
    {
      MBWM_DBG ("### is input ###\n");
      return mb_wm_client_input_new (wm, win);
    }
  else if (win->net_type == wm->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_NORMAL])
    {
      MBWM_DBG ("### is application ###\n");
      return mb_wm_client_app_new (wm, win);
    }
/*************************
  @@@ TODO: this could be very much simplified with a table mapping
  @@@ atoms to constructors
**************************/
  else
    {
      char * name = XGetAtomName (wm->xdpy, win->net_type);
      g_warning ("### unhandled window type %s (%lx) ###",
	     name?name:"[no net_type]",
	     win->xwindow);
      XFree (name);
      return mb_wm_client_app_new (wm, win);
    }

  return NULL;
}

static MBWMTheme *
mb_wm_real_theme_new (MBWindowManager * wm, const char * path)
{
  /*
   *  FIXME -- load the selected theme from some configuration
   */
  return mb_wm_theme_new (wm, path);
}

#if ENABLE_COMPOSITE && COMP_MGR_BACKEND
static MBWMCompMgr *
mb_wm_real_comp_mgr_new (MBWindowManager *wm)
{
#if ENABLE_CLUTTER_COMPOSITE_MANAGER
  return mb_wm_comp_mgr_clutter_new (wm);
#else
  return mb_wm_comp_mgr_xrender_new (wm);
#endif
}
#endif

static MBWMLayout *
mb_wm_layout_new_real (MBWindowManager *wm)
{
  MBWMLayout * layout = mb_wm_layout_new (wm);

  if (!layout)
    mb_wm_util_fatal_error("OOM?");

  return layout;
}

#if USE_GTK
static GdkFilterReturn
mb_wm_gdk_xevent_filter (GdkXEvent *xevent, GdkEvent *event, gpointer data)
{
  MBWindowManager * wm = data;
  XEvent          * xev = (XEvent*) xevent;

  mb_wm_main_context_handle_x_event (xev, wm->main_ctx);

  if (wm->sync_type)
    mb_wm_sync (wm);

  return GDK_FILTER_CONTINUE;
}
#endif

#if ENABLE_CLUTTER_COMPOSITE_MANAGER
#if USE_GTK
static GdkFilterReturn
mb_wm_clutter_gdk_xevent_filter (GdkXEvent *xevent, GdkEvent *event,
				 gpointer data)
{
  switch (clutter_x11_handle_event ((XEvent*)xevent))
    {
    default:
    case CLUTTER_X11_FILTER_CONTINUE:
      return GDK_FILTER_CONTINUE;
    case CLUTTER_X11_FILTER_TRANSLATE:
      return GDK_FILTER_TRANSLATE;
    case CLUTTER_X11_FILTER_REMOVE:
      return GDK_FILTER_REMOVE;
    }

  return GDK_FILTER_CONTINUE;
}
#else
static ClutterX11FilterReturn
mb_wm_clutter_xevent_filter (XEvent *xev, ClutterEvent *cev, gpointer data)
{
  MBWindowManager * wm = data;

  mb_wm_main_context_handle_x_event (xev, wm->main_ctx);

  if (wm->sync_type)
    mb_wm_sync (wm);

  return CLUTTER_X11_FILTER_CONTINUE;
}
#endif
#endif

#if USE_GTK
#include <gtk/gtk.h>
#endif

#if ENABLE_CLUTTER_COMPOSITE_MANAGER || USE_GTK
static void
mb_wm_main_real (MBWindowManager *wm)
{

#if USE_GTK
  gdk_window_add_filter (NULL, mb_wm_gdk_xevent_filter, wm);
#if ENABLE_CLUTTER_COMPOSITE_MANAGER
  gdk_window_add_filter (NULL, mb_wm_clutter_gdk_xevent_filter, NULL);
#endif
  gtk_main ();
#else
  clutter_x11_add_filter (mb_wm_clutter_xevent_filter, wm);
  clutter_main ();
#endif
}
#endif

static void
mb_wm_class_init (MBWMObjectClass *klass)
{
  MBWindowManagerClass *wm_class;

  MBWM_MARK();

  wm_class = (MBWindowManagerClass *)klass;

  wm_class->process_cmdline = mb_wm_process_cmdline;
  wm_class->client_new      = mb_wm_client_new_func;
  wm_class->theme_new       = mb_wm_real_theme_new;
  wm_class->client_activate = mb_wm_activate_client_real;
  wm_class->layout_new      = mb_wm_layout_new_real;

#if ENABLE_CLUTTER_COMPOSITE_MANAGER
  wm_class->main            = mb_wm_main_real;
#endif

#if ENABLE_COMPOSITE && COMP_MGR_BACKEND
  wm_class->comp_mgr_new    = mb_wm_real_comp_mgr_new;
#endif

#if MBWM_WANT_DEBUG
  klass->klass_name = "MBWindowManager";
#endif
}

static void
mb_wm_destroy (MBWMObject *this)
{
  MBWindowManager * wm = MB_WINDOW_MANAGER (this);
  MBWMList *l = wm->clients;

  while (l)
    {
      MBWMList * old = l;
      mb_wm_object_unref (l->data);

      l = l->next;

      free (old);
    }

  mb_wm_object_unref (MB_WM_OBJECT (wm->root_win));
  mb_wm_object_unref (MB_WM_OBJECT (wm->theme));
  mb_wm_object_unref (MB_WM_OBJECT (wm->layout));
  mb_wm_object_unref (MB_WM_OBJECT (wm->main_ctx));
}

static int
mb_window_manager_init (MBWMObject *this, va_list vap);

int
mb_wm_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (MBWindowManagerClass),
	sizeof (MBWindowManager),
	mb_window_manager_init,
	mb_wm_destroy,
	mb_wm_class_init
      };

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_OBJECT, 0);
    }

  return type;
}

MBWindowManager*
mb_wm_new (int argc, char **argv)
{
  MBWindowManager      *wm = NULL;

  wm = MB_WINDOW_MANAGER (mb_wm_object_new (MB_TYPE_WINDOW_MANAGER,
					    MBWMObjectPropArgc, argc,
					    MBWMObjectPropArgv, argv,
					    NULL));

  if (!wm)
    return wm;

  return wm;
}

MBWindowManager*
mb_wm_new_with_dpy (int argc, char **argv, Display * dpy)
{
  MBWindowManager      *wm = NULL;

  wm = MB_WINDOW_MANAGER (mb_wm_object_new (MB_TYPE_WINDOW_MANAGER,
					    MBWMObjectPropArgc, argc,
					    MBWMObjectPropArgv, argv,
					    MBWMObjectPropDpy,  dpy,
					    NULL));

  if (!wm)
    return wm;

  return wm;
}

static Bool
mb_wm_handle_key_press (XKeyEvent       *xev,
			void            *userdata)
{
  MBWindowManager *wm = (MBWindowManager*)userdata;

  mb_wm_keys_press (wm,
		    XkbKeycodeToKeysym(wm->xdpy, xev->keycode, 0, 0),
		    xev->state);

  return True;
}

static Bool
mb_wm_handle_destroy_notify (XDestroyWindowEvent  *xev,
			     void                 *userdata)
{
  MBWindowManager       *wm = (MBWindowManager*)userdata;
  MBWindowManagerClient *client = NULL;

  MBWM_MARK();

  client = mb_wm_managed_client_from_xwindow(wm, xev->window);

  if (client)
    {
      if (mb_wm_client_window_is_state_set (client->window,
					    MBWMClientWindowEWMHStateHidden))
	{
	  if (mb_wm_client_is_hiding_from_desktop (client))
	    {
	      /*
	       * If the destroyed hidden window is hidden because it was
	       * on a different desktop than current, we have to unmanage
	       * it, as it is still considered managed.
	       */
	      mb_wm_unmanage_client (wm, client, True);
	    }
	  else
	    {
	      /*
	       * Otherwise, this is regular minimized window, in which case
	       * the window is no longer managed, only the resources are
	       * kept in the clients list; so we only remove it and free.
	       */
	      wm->clients = mb_wm_util_list_remove (wm->clients, client);
	      mb_wm_object_unref (MB_WM_OBJECT (client));
	    }
	}
      else
	mb_wm_unmanage_client (wm, client, True);
    }

  return True;
}

static Bool
mb_wm_handle_unmap_notify (XUnmapEvent          *xev,
			   void                 *userdata)
{
  MBWindowManager       *wm = (MBWindowManager*)userdata;
  MBWindowManagerClient *client = NULL;

  MBWM_MARK();

  /* Ignoring syntetic events, not even decrementing the skip_unmaps counter. */
  if (xev->send_event)
    return True;

  /*
   * When the XCompositeRedirectWindow() is used we get an extra unmap event
   * which is filtered out here. We will have an other event about the redirects
   * with the xany.window set to the parent window.
   */
  if (xev->window == xev->event)
    return True;

  client = mb_wm_managed_client_from_xwindow(wm, xev->window);

  if (client)
    {
      if (mb_wm_client_ping_in_progress (client))
        {
          MBWindowManagerClass  *wm_klass;

	  /* stop ping process since the client closed the window */
          mb_wm_client_ping_stop (client);

          wm_klass = MB_WINDOW_MANAGER_CLASS (MB_WM_OBJECT_GET_CLASS (wm));

          if (wm_klass->client_responding)
	    wm_klass->client_responding (wm, client);
        }

      if (client->skip_unmaps)
	{
	  MBWM_DBG ("skipping unmap for %p (skip count %d)\n",
		    client, client->skip_unmaps);

          /* When we skip an unmap, skip a map too. */
	  client->skip_unmaps--;
          client->skip_maps++;
          MB_WM_DBG_SKIP_UNMAPS (client);
	}
      else
	{
	  /*
	   * If the iconizing flag is set, we unmanage the client, but keep
	   * the resources around; we reset the iconizing flag to indicate
	   * that the iconizing has completed (and the client window is now in
	   * hidden state).
	   *
	   * If the client is not iconizing and is not alreadly in a hidden
	   * state, we unmange it and destroy all the associated resources.
	   */
          mb_wm_client_set_map_confirmed (client, False);
          mb_wm_client_set_unmap_confirmed (client, True);

	  if (mb_wm_client_is_iconizing (client))
	    {
	      MBWM_DBG ("iconizing client %p\n", client);

	      mb_wm_unmanage_client (wm, client, False);
	      mb_wm_client_reset_iconizing (client);
	    }
	  else if (!mb_wm_client_window_is_state_set (client->window,
					    MBWMClientWindowEWMHStateHidden))
	    {
	      mb_wm_client_remove_all_transients (client);

#if ENABLE_COMPOSITE
	      if (mb_wm_compositing_enabled (wm))
		mb_wm_comp_mgr_unmap_notify (wm->comp_mgr, client);
#endif
	      MBWM_DBG ("removing client %p\n", client);
	      mb_wm_unmanage_client (wm, client, True);
	    }
	}
    }

  return True;
}

static Bool
mb_wm_handle_property_notify (XPropertyEvent          *xev,
			      void                    *userdata)
{
  MBWindowManager       *wm = (MBWindowManager*)userdata;
  MBWindowManagerClient *client;
  int flag = 0;

  if (xev->window == wm->root_win->xwindow)
    {
      if (xev->atom == wm->atoms[MBWM_ATOM_MB_THEME])
	{
	  Atom type;
	  int  format;
	  unsigned long items;
	  unsigned long left;
	  unsigned char *theme_path;
	  Bool           same_path;

	  XGetWindowProperty (wm->xdpy, wm->root_win->xwindow,
			      xev->atom, 0, 8192, False,
			      XA_STRING, &type, &format,
			      &items, &left,
			      &theme_path);

	  if (!type || !items)
	    return True;

	  /*
	   * With protecting/unprotecting of the theme we can sense if the WM
	   * segfaults because of a broken theme. However if the theme path is
	   * the same we don't want to unprotect the theme if it is considered
	   * to be broken. We only unprotect when the user sets a new theme to
	   * be used.
	   */
	  same_path = theme_path && wm->theme_path &&
		  strcmp ((char *)theme_path, wm->theme_path) == 0;
	  if (!same_path)
	    mb_wm_theme_protect ();
	  mb_wm_set_theme_from_path (wm, (char*)theme_path);
	  if (!same_path)
            mb_wm_theme_unprotect ();

	  XFree (theme_path);
	}
      else if (xev->atom == wm->atoms[MBWM_ATOM_MAEMO_SUPPRESS_ROOT_RECONFIGURATION])
        {
          static unsigned orig_width, orig_height;

          /*
           * hildon-desktop sets this property before and after calling RandR.
           * We may or may not get ConfigureNotify about the root window.
           * If not, we need to emulate it (so we resize the affected windows,
           * ie. the ones supporting portrait-mode), but if we do, make sure
           * we don't interfere.  Let us assume the toggle is initially off.
           * If it's not hildon-desktop is borked anyway.
           */
          if (xev->state == PropertyNewValue)
            { /* We're before rotation. */
              orig_width  = wm->xdpy_width;
              orig_height = wm->xdpy_height;
            }
          else if (wm->xdpy_width != orig_height && wm->xdpy_height != orig_width)
            {
              XConfigureEvent fake;

              /* We're after rotation, and the root window hasn't been
               * reconfigured it seems. */
              fake.width = orig_height;
              fake.height = orig_width;
              mb_wm_handle_root_config_notify (&fake, wm);
            }
        }

      return True;
    }

  client = mb_wm_managed_client_from_xwindow(wm, xev->window);

  if (!client)
    return True;

  if (xev->atom == wm->atoms[MBWM_ATOM_NET_WM_USER_TIME])
    flag = MBWM_WINDOW_PROP_NET_USER_TIME;
  else if (xev->atom == wm->atoms[MBWM_ATOM_WM_NAME] ||
	   xev->atom == wm->atoms[MBWM_ATOM_NET_WM_NAME] ||
	   xev->atom == wm->atoms[MBWM_ATOM_HILDON_WM_NAME])
    flag = MBWM_WINDOW_PROP_NAME;
  else if (xev->atom == wm->atoms[MBWM_ATOM_WM_HINTS])
    flag = MBWM_WINDOW_PROP_WM_HINTS;
  else if (xev->atom == wm->atoms[MBWM_ATOM_WM_PROTOCOLS])
    flag = MBWM_WINDOW_PROP_PROTOS;
  else if (xev->atom == wm->atoms[MBWM_ATOM_WM_TRANSIENT_FOR])
    flag = MBWM_WINDOW_PROP_TRANSIENCY;
  else if (xev->atom == wm->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE])
    flag = MBWM_WINDOW_PROP_WIN_TYPE;
  else if (xev->atom == wm->atoms[MBWM_ATOM_WM_CLIENT_MACHINE])
    flag = MBWM_WINDOW_PROP_CLIENT_MACHINE;
  else if (xev->atom == wm->atoms[MBWM_ATOM_NET_WM_PID])
    flag = MBWM_WINDOW_PROP_NET_PID;
  else if (xev->atom == wm->atoms[MBWM_ATOM_HILDON_STACKING_LAYER])
    flag = MBWM_WINDOW_PROP_HILDON_STACKING;
  else if (xev->atom == wm->atoms[MBWM_ATOM_HILDON_PORTRAIT_MODE_SUPPORT])
    flag = MBWM_WINDOW_PROP_PORTRAIT;
  else if (xev->atom == wm->atoms[MBWM_ATOM_HILDON_PORTRAIT_MODE_REQUEST])
    flag = MBWM_WINDOW_PROP_PORTRAIT;
  else if (xev->atom == wm->atoms[MBWM_ATOM_HILDON_LIVE_DESKTOP_BACKGROUND])
    flag = MBWM_WINDOW_PROP_LIVE_BACKGROUND;

  if (flag)
    mb_wm_client_window_sync_properties (client->window, flag);

  return True;
}

#if ENABLE_COMPOSITE
static  Bool
mb_wm_handle_composite_config_notify (XConfigureEvent *xev,
			    void            *userdata)
{
  MBWindowManager * wm = (MBWindowManager*)userdata;

  if (mb_wm_comp_mgr_enabled (wm->comp_mgr))
    {
      MBWindowManagerClient *client;

      client = mb_wm_managed_client_from_frame (wm, xev->window);

      if (client)
	mb_wm_comp_mgr_client_configure (client->cm_client);
    }
  return True;
}
#endif

/*
 * This is called if the root window resizes itself, which happens when RANDR is
 * used to resize or rotate the display.
 */
static  Bool
mb_wm_handle_root_config_notify (XConfigureEvent *xev,
			    void            *userdata)
{
  MBWindowManager * wm = (MBWindowManager*)userdata;
  MBWindowManagerClient * c;

  /* We get some spurious events from X here, so just make sure
   * to ignore them or we spend ages checking window sizes on rotation. */
  if (wm->xdpy_width == xev->width && wm->xdpy_height == xev->height)
    return True;

  wm->xdpy_width = xev->width;
  wm->xdpy_height = xev->height;

#if ENABLE_COMPOSITE
  if (wm->comp_mgr && !wm->comp_mgr->disabled)
    {
      MBWMCompMgrClass *k;

      k = MB_WM_COMP_MGR_CLASS (MB_WM_OBJECT_GET_CLASS (wm->comp_mgr));
      if (k->screen_size_changed)
        k->screen_size_changed (wm->comp_mgr,
                                wm->xdpy_width, wm->xdpy_height);
    }
#endif

  /* Bastard hack part 2: now that the screen is reconfigured
   * map and activate the client which caused us to rotate. */
  for (c = wm->stack_top; c; c = c->stacked_below)
    if (!mb_wm_client_is_map_confirmed (c) && c->window->portrait_requested > 0)
      {
        mb_wm_activate_client (wm, c);
        break;
      }

  mb_wm_display_sync_queue (wm, MBWMSyncGeometry);
  return True;
}

static  Bool
mb_wm_handle_config_request (XConfigureRequestEvent *xev,
			     void                   *userdata)
{
  MBWindowManager       *wm = (MBWindowManager*)userdata;
  MBWindowManagerClient *client;
  unsigned long          value_mask;
  int                    req_x = xev->x;
  int                    req_y = xev->y;
  int                    req_w = xev->width;
  int                    req_h = xev->height;

  client = mb_wm_managed_client_from_xwindow(wm, xev->window);

  if (!client)
    {
      XWindowChanges  xwc;
      MBWM_DBG ("### No client found for configure event ###\n");
      /*
       * We have to allow this window to configure; things like gtk menus
       * and hildon banners all request configuration before mapping, so
       * if we did not allow this, things break down.
       */
      xwc.x          = req_x;
      xwc.y          = req_y;
      xwc.width      = req_w;
      xwc.height     = req_h;
      xwc.sibling    = xev->above;
      xwc.stack_mode = xev->detail;

      MB_WM_DBG_MOVE_RESIZE ("noclient", xev->window, &xwc);
      XConfigureWindow (wm->xdpy, xev->window, xev->value_mask, &xwc);

      return True;
    }

  value_mask = xev->value_mask;

  if (client->window)
    {
      MBGeometry req_geom,
	*win_geom = &client->window->geometry;

      req_geom.x      = (value_mask & CWX)      ? req_x : win_geom->x;
      req_geom.y      = (value_mask & CWY)      ? req_y : win_geom->y;
      req_geom.width  = (value_mask & CWWidth)  ? req_w : win_geom->width;
      req_geom.height = (value_mask & CWHeight) ? req_h : win_geom->height;

      /* We can't determine at this point what the right response
       * to this configure request is since layout management might
       * also want to tweak the window geometry.
       *
       * We make a note that the configure request needs a response
       * and when we reach mb_wm_sync - but after all layout decisions
       * have been made - then we can determine if the request
       * has been accepted or not and send any synthetic events as
       * needed.
       */
      mb_wm_client_configure_request_ack_queue (client);
      MB_WM_DBG_MOVE_RESIZE ("request", client->window->xwindow, &req_geom);
      mb_wm_client_request_geometry (client,
				     &req_geom,
				     MBWMClientReqGeomIsViaConfigureReq);
    }

  return True;
}

/*
 * Check if this window belongs to the WM, and if it does, and is a client
 * window, optionaly return the client.
 */
static Bool
mb_wm_is_my_window (MBWindowManager *wm,
		    Window xwin,
		    MBWindowManagerClient **client)
{
  MBWindowManagerClient *c;

#if ENABLE_COMPOSITE
  if (wm->comp_mgr && mb_wm_comp_mgr_is_my_window (wm->comp_mgr, xwin))
    {
      /* Make sure to set the returned client to NULL, as this is a
       * window that belongs to the composite manager, and so it has no
       * client associated with it.
       */
      if (client)
	*client = NULL;

      return True;
    }
#endif

  mb_wm_stack_enumerate_reverse(wm, c)
    if (mb_wm_client_owns_xwindow (c, xwin))
      {
	if (client)
	  *client = c;

	return True;
      }

  return False;
}

#if ENABLE_COMPOSITE

static void
mb_wm_unredirect_client (MBWindowManager *wm,
                         MBWindowManagerClient *client)
{
  if (client->cm_client)
    mb_wm_comp_mgr_clutter_set_client_redirection (client->cm_client, FALSE);

  if (client->xwin_frame)
    {
      XCompositeUnredirectWindow (wm->xdpy, client->xwin_frame,
                                  CompositeRedirectManual);
      XCompositeUnredirectSubwindows (wm->xdpy, client->xwin_frame,
                                      CompositeRedirectManual);
    }
}

void
mb_wm_setup_redirection (MBWindowManager *wm, int redirection)
{
  MBWindowManagerClient *c;
  Window root_win = wm->root_win->xwindow;

  if (redirection)
  {
    for (c = wm->stack_bottom; c; c = c->stacked_above)
      if (c->cm_client)
        mb_wm_comp_mgr_clutter_set_client_redirection (c->cm_client, TRUE);

    XCompositeRedirectSubwindows (wm->xdpy, root_win,
                                  CompositeRedirectManual);
    wm->non_redirection = False;
  }
  else
  {
    for (c = wm->stack_bottom; c; c = c->stacked_above)
      if (c->cm_client)
        mb_wm_comp_mgr_clutter_set_client_redirection (c->cm_client, FALSE);

    XCompositeUnredirectSubwindows (wm->xdpy, root_win,
                                    CompositeRedirectManual);
    wm->non_redirection = True;
  }
}

/*  For the compositing engine we need to track overide redirect
 *  windows so the compositor can paint them.
 */
static Bool
mb_wm_handle_map_notify   (XMapEvent  *xev,
			   void       *userdata)
{
  MBWindowManager       *wm = (MBWindowManager*)userdata;
  MBWindowManagerClient *client = NULL;
  MBWindowManagerClass  *wm_class =
    MB_WINDOW_MANAGER_CLASS (MB_WM_OBJECT_GET_CLASS (wm));
  MBWMClientWindow *win = NULL;
  XWindowAttributes attrs = { 0 };
  int err;

  g_debug ("%s: @@@@ Map Notify for %lx @@@@", __func__, xev->window);

  /* For the same reason as in mb_wm_handle_unmap_notify(). */
  if (xev->window == xev->event)
    return True;

  if (!wm_class->client_new)
    {
      MBWM_DBG("### No new client hook exists ###");
      return True;
    }

  if (mb_wm_is_my_window (wm, xev->window, &client))
    {
      if (client && client->skip_maps)
        {
          client->skip_maps--;
          MB_WM_DBG_SKIP_UNMAPS (client);

          /* we have to unredirect again because the reparenting redirected
           * the client again */
          if (client->cm_client &&
              mb_wm_comp_mgr_clutter_client_is_unredirected (client->cm_client))
            mb_wm_comp_mgr_clutter_set_client_redirection (client->cm_client,
                                                           FALSE);
        }
      else if (client)
	{
	  /*
	   * Only notify the CM when the top-level window maps, not for the
	   * decors, etc.
	   */
	  if (xev->window == client->window->xwindow && wm->comp_mgr)
	    {
	      MBWM_NOTE (COMPOSITOR, "@@@@ client %p @@@@\n", client);
	      mb_wm_comp_mgr_map_notify (wm->comp_mgr, client);

	      /*
	       * If the hiding_from_deskotp flag is set, reset it
	       */
	      mb_wm_client_reset_hiding_from_desktop (client);
	    }

          if (wm->non_redirection)
            mb_wm_unredirect_client (wm, client);

	  mb_wm_client_set_map_confirmed (client, True);
	}

      return True;
    }

  /* We don't care about X errors here, because they will be reported
   * in the return value of XGetWindowAttributes. */
  mb_wm_util_async_trap_x_errors (wm->xdpy);
  err = XGetWindowAttributes(wm->xdpy, xev->window, &attrs);
  mb_wm_util_async_untrap_x_errors();
  if (!err)
    {
      g_debug ("%s: XGetWindowAttributes for %lx failed with code %d",
                 __FUNCTION__, xev->window, err);
      return True;
    }

  if (!attrs.override_redirect)
    {
      g_debug ("%s: unmap for %lx has happened after MapRequest",
               __FUNCTION__, xev->window);
      return True;
    }

  win = mb_wm_client_window_new (wm, xev->window);

  if (!win)
    {
      g_debug ("%s: mb_wm_client_window_new failed", __FUNCTION__);
      return True;
    }

  if (win->window_class == InputOnly)
    {
      mb_wm_object_unref (MB_WM_OBJECT (win));
      return True;
    }

  client = wm_class->client_new (wm, win);

  if (!client)
    {
      mb_wm_object_unref (MB_WM_OBJECT (win));
      return True;
    }

  mb_wm_manage_client (wm, client, True);
  mb_wm_comp_mgr_map_notify (wm->comp_mgr, client);

  if (wm->non_redirection)
    mb_wm_unredirect_client (wm, client);

  mb_wm_client_set_map_confirmed (client, True);
  return True;
}
#endif

static Bool
mb_wm_handle_map_request (XMapRequestEvent  *xev,
			  void              *userdata)
{
  MBWindowManager       *wm = (MBWindowManager*)userdata;
  MBWindowManagerClient *client = NULL;
  MBWindowManagerClass  *wm_class =
    MB_WINDOW_MANAGER_CLASS (MB_WM_OBJECT_GET_CLASS (wm));
  MBWMClientWindow *win = NULL;

  MBWM_MARK();

  g_debug ("%s: @@@@ Map Request for %lx @@@@", __func__, xev->window);

  if (mb_wm_is_my_window (wm, xev->window, &client))
    {
      if (client)
	mb_wm_activate_client (wm, client);

      return True;
    }

  if (!wm_class->client_new)
    {
      MBWM_DBG("### No new client hook exists ###");
      return True;
    }

  win = mb_wm_client_window_new (wm, xev->window);

  if (!win)
    return True;

  client = wm_class->client_new (wm, win);

  if (!client)
    {
      mb_wm_object_unref (MB_WM_OBJECT (win));
      return True;
    }

  if (mb_wm_client_window_is_state_set (win, MBWMClientWindowEWMHStateFullscreen))
    mb_wm_client_fullscreen_mark_dirty (client);

  mb_wm_manage_client (wm, client, True);

  return True;
}


static void
stack_get_window_list (MBWindowManager *wm, Window * win_list, int * count)
{
  MBWindowManagerClient *client;
  int                    i = 0;

  if (!wm->stack_n_clients)
    return;

  mb_wm_stack_enumerate_reverse(wm, client)
  {
    if (client->xwin_frame &&
	!(client->window->ewmh_state & MBWMClientWindowEWMHStateFullscreen))
      win_list[i++] = client->xwin_frame;
    else
      win_list[i++] = MB_WM_CLIENT_XWIN(client);

    if (client->xwin_modal_blocker)
      win_list[i++] = client->xwin_modal_blocker;
  }

  *count = i;
}

static void
stack_sync_to_display (MBWindowManager *wm)
{
  Window *win_list = NULL;
  int count = 0;

  if (!wm->stack_n_clients)
    return;

  /*
   * Allocate two slots for each client; this guarantees us enough space for
   * both client windows and any modal blockers without having to keep track
   * of how many of the blocker windows we have (the memory overhead for this
   * is negligeable and very short lived)
   */
  win_list = alloca (sizeof(Window) * (wm->stack_n_clients * 2));

  stack_get_window_list(wm, win_list, &count);

  mb_wm_util_async_trap_x_errors_warn(wm->xdpy, "XRestackWindows");
  XRestackWindows(wm->xdpy, win_list, count);
  mb_wm_util_async_untrap_x_errors();
}

static void
mb_wm_focus_client_as_stacked (MBWindowManager *wm,
                               MBWindowManagerClient *except_client)
{
  extern gboolean hd_dbus_tklock_on;

  wm->focused_client = NULL;
  if (!hd_dbus_tklock_on)
    {
      MBWindowManagerClient *c;

      for (c = wm->stack_top; c; c = c->stacked_below)
        {
          /* do not assign focus to Home applets automatically */
          if (c != except_client &&
              mb_wm_client_want_focus (c) &&
              mb_wm_client_is_visible (c) &&
              c->window->net_type !=
                wm->atoms[MBWM_ATOM_HILDON_WM_WINDOW_TYPE_HOME_APPLET])
            {
              mb_wm_focus_client (wm, c);
              if (wm->focused_client || wm->focus_after_stacking)
                /* focused or will focus something */
                return;
              else
                /* arguably we could continue searching */
                break;
            }
          if (mb_wm_client_covers_screen (c))
            /* anything below this is necessarily invisible */
            break;
        }
    } else
      /* don't give focus back to any already mapped window
       * because the touch screen is still locked and we don't want
       * the application to wake up */;

  /* Make sure the focus is not None, otherwise we won't get KeyPressed
   * events and shortcurs will stop working. */
  XSetInputFocus(wm->xdpy, wm->root_win->xwindow, RevertToPointerRoot,
                 CurrentTime);
}

void
mb_wm_sync (MBWindowManager *wm)
{
  /* Sync all changes to display */
  MBWindowManagerClient *client = NULL;
#ifdef TIME_MB_WM_SYNC
  GTimer *timer = g_timer_new();
#endif
  MBWM_MARK();
  MBWM_TRACE ();

  XGrabServer(wm->xdpy);

  /* First of all, make sure stack is correct */
  if (wm->sync_type & MBWMSyncStacking)
    {
      mb_wm_stack_ensure (wm);

#if ENABLE_COMPOSITE
      if (wm->comp_mgr && mb_wm_comp_mgr_enabled (wm->comp_mgr))
	mb_wm_comp_mgr_restack (wm->comp_mgr);
#endif
    }

  /* Size stuff first assume newly managed windows unmapped ?
   *
   */
  if (wm->layout && (wm->sync_type & MBWMSyncGeometry))
    mb_wm_layout_update (wm->layout);

  /* Create the actual windows */
  mb_wm_stack_enumerate(wm, client)
    if (!mb_wm_client_is_realized (client))
      mb_wm_client_realize (client);

  /*
   * Now do updates per individual client - maps, paints etc, main work here
   *
   * If an item in the stack needs visibilty sync, then we have to force it
   * for all items that are above it on the stack.
   */
  mb_wm_stack_enumerate(wm, client)
    if (mb_wm_client_needs_sync (client))
      mb_wm_client_display_sync (client);

#if ENABLE_COMPOSITE
  if (mb_wm_comp_mgr_enabled (wm->comp_mgr))
    mb_wm_comp_mgr_render (wm->comp_mgr);
#endif

  /* FIXME: optimise wm sync flags so know if this needs calling */
  /* FIXME: Can we restack an unmapped window ? - problem of new
   *        clients mapping below existing ones.
  */
  if (wm->sync_type & MBWMSyncStacking)
    stack_sync_to_display (wm);

  /* FIXME: New clients now managed will likely need some propertys
   *        synced up here.
  */

  XUngrabServer(wm->xdpy);
  XFlush(wm->xdpy);
  wm->sync_type = 0;

  if (wm->focus_after_stacking)
    {
      wm->focus_after_stacking = False;
      mb_wm_focus_client_as_stacked (wm, NULL);
    }

#ifdef TIME_MB_WM_SYNC
  g_debug("mb_wm_sync: %f", g_timer_elapsed(timer,0));
  g_timer_destroy(timer);
#endif
}

static void
mb_wm_update_root_win_lists (MBWindowManager *wm)
{
  Window root_win = wm->root_win->xwindow;

  if (!mb_wm_stack_empty(wm))
    {
      Window                *wins = NULL;
      int                    cnt = 0;
      int                    list_size;
      MBWindowManagerClient *c;
      MBWMList              *l;

      list_size     = mb_wm_util_list_length (wm->clients);

      wins      = alloca (sizeof(Window) * list_size);

      if ((wm->flags & MBWindowManagerFlagDesktop) && wm->desktop)
	{
	  wins[cnt++] = MB_WM_CLIENT_XWIN(wm->desktop);
	}

      mb_wm_stack_enumerate (wm,c)
	{
	  if (!(wm->flags & MBWindowManagerFlagDesktop) || c != wm->desktop)
	    wins[cnt++] = c->window->xwindow;
	}

      XChangeProperty(wm->xdpy, root_win,
		      wm->atoms[MBWM_ATOM_NET_CLIENT_LIST_STACKING],
		      XA_WINDOW, 32, PropModeReplace,
		      (unsigned char *)wins, cnt);

      /* Update _NET_CLIENT_LIST but with 'age' order rather than stacking */
      cnt = 0;
      l = wm->clients;
      while (l)
	{
	  c = l->data;
	  wins[cnt++] = c->window->xwindow;

	  l = l->next;
	}

      XChangeProperty(wm->xdpy, root_win,
		      wm->atoms[MBWM_ATOM_NET_CLIENT_LIST] ,
		      XA_WINDOW, 32, PropModeReplace,
		      (unsigned char *)wins, cnt);
    }
  else
    {
      /* No managed windows */
      XChangeProperty(wm->xdpy, root_win,
		      wm->atoms[MBWM_ATOM_NET_CLIENT_LIST_STACKING] ,
		      XA_WINDOW, 32, PropModeReplace,
		      NULL, 0);

      XChangeProperty(wm->xdpy, root_win,
		      wm->atoms[MBWM_ATOM_NET_CLIENT_LIST] ,
		      XA_WINDOW, 32, PropModeReplace,
		      NULL, 0);
    }
}

static void
mb_wm_manage_client (MBWindowManager       *wm,
		     MBWindowManagerClient *client,
		     Bool                   activate)
{
  /* Add to our list of managed clients */
  MBWMSyncType sync_flags = MBWMSyncVisibility | MBWMSyncGeometry;

  if (client == NULL)
    return;

  wm->clients = mb_wm_util_list_append(wm->clients, (void*)client);

  /* add to stack and move to position in stack */
  mb_wm_stack_append_top (client);
  mb_wm_client_stack(client, 0);
  mb_wm_update_root_win_lists (wm);

  if (MB_WM_CLIENT_CLIENT_TYPE (client) == MBWMClientTypePanel)
    {
      mb_wm_update_root_win_rectangles (wm);
      mb_wm_client_set_desktop (client, -1);
    }
  else if (MB_WM_CLIENT_CLIENT_TYPE (client) == MBWMClientTypeDesktop)
    {
      wm->desktop = client;
      mb_wm_client_set_desktop (client, -1);
    }
  else if (client->transient_for)
    {
      /*
       * For transient clients, set the desktop to that of the top level
       * parent; if this does not match the active desktop, hide the client.
       */
      MBWindowManagerClient * parent = client->transient_for;
      int                     desktop;

      while (parent->transient_for)
	parent = parent->transient_for;

      desktop = mb_wm_client_get_desktop (parent);

      mb_wm_client_set_desktop (client, desktop);

      if (desktop != wm->active_desktop)
	mb_wm_client_desktop_change (client, wm->active_desktop);
    }
  else
    mb_wm_client_set_desktop (client, wm->active_desktop);

  /*
   * Must not mess with stacking if the client if is of the override type
   */
  if (MB_WM_CLIENT_CLIENT_TYPE (client) != MBWMClientTypeOverride)
    sync_flags |= MBWMSyncStacking;

  mb_wm_comp_mgr_register_client (wm->comp_mgr, client, activate);

  mb_wm_display_sync_queue (client->wmref, sync_flags);
}

/*
 * destroy indicates whether the client, if it is an application,
 * should be destroyed or moved into the iconized category.
 */
static void
mb_wm_unmanage_client (MBWindowManager       *wm,
		       MBWindowManagerClient *client,
		       Bool                   destroy)
{
  /* FIXME: set a managed flag in client object ? */
  MBWMClientType c_type = MB_WM_CLIENT_CLIENT_TYPE (client);
  MBWMSyncType sync_flags = 0;

  /* Restack even if an override-redirected window is being unmanaged,
   * so hildon-desktop will have a chance to restore fullscreenness
   * if it had to take away the title bar. */
  sync_flags |= MBWMSyncStacking;

  if (c_type & (MBWMClientTypePanel | MBWMClientTypeInput))
    sync_flags |= MBWMSyncGeometry;

  if (destroy)
    wm->clients = mb_wm_util_list_remove (wm->clients, (void*)client);

  mb_wm_stack_remove (client);
  mb_wm_update_root_win_lists (wm);

  if (MB_WM_CLIENT_CLIENT_TYPE (client) == MBWMClientTypePanel)
    mb_wm_update_root_win_rectangles (wm);

  /*
   * Must remove client from any transient list, otherwise when we call
   * _stack_enumerate() everything will go pearshape
   */
  mb_wm_client_detransitise (client);

#if ENABLE_COMPOSITE
  if (mb_wm_comp_mgr_enabled (wm->comp_mgr))
    {
      /*
       * If destroy == False, this unmap was triggered by iconizing the
       * client; in that case, we do not destory the CM client data, only
       * make sure the client is hidden (note that any 'minimize' effect
       * has already completed by the time we get here).
       */
      if (destroy)
	mb_wm_comp_mgr_unregister_client (wm->comp_mgr, client);
      else
	mb_wm_comp_mgr_client_hide (client->cm_client);
    }
#endif

  if (wm->focused_client == client)
    mb_wm_unfocus_client (wm, client);

  if (client == wm->desktop)
    wm->desktop = NULL;

  if (destroy)
    mb_wm_object_unref (MB_WM_OBJECT(client));

  mb_wm_display_sync_queue (wm, sync_flags);
}

MBWindowManagerClient*
mb_wm_managed_client_from_xwindow(MBWindowManager *wm, Window win)
{
  MBWindowManagerClient *client = NULL;
  MBWMList *l;

  if (win == wm->root_win->xwindow)
    return NULL;

  l = wm->clients;
  while (l)
    {
      client = l->data;

      if (client->window && client->window->xwindow == win)
	return client;

      l = l->next;
    }

  return NULL;
}

MBWindowManagerClient*
mb_wm_managed_client_from_frame (MBWindowManager *wm, Window frame)
{
  MBWindowManagerClient *client = NULL;
  MBWMList *l;

  if (frame == wm->root_win->xwindow)
    return NULL;

  l = wm->clients;
  while (l)
    {
      client = l->data;

      if (mb_wm_client_owns_xwindow (client, frame))
	return client;

      l = l->next;
    }

  return NULL;
}

/*
 * Run the main loop; there are three options dependent on how we were
 * configured at build time:
 *
 * * If configured without glib main loop integration, we defer to our own
 *   main loop implementation provided by MBWMMainContext.
 *
 * * If configured with glib main loop integration:
 *
 *   * If there is an implemetation for the MBWindowManager main() virtual
 *     function, we call it.
 *
 *   * Otherwise, start a normal glib main loop.
 */
void
mb_wm_main_loop(MBWindowManager *wm)
{
#if !USE_GLIB_MAINLOOP
  mb_wm_main_context_loop (wm->main_ctx);
#else
  MBWindowManagerClass * wm_class =
    MB_WINDOW_MANAGER_CLASS (MB_WM_OBJECT_GET_CLASS (wm));

#if !ENABLE_CLUTTER_COMPOSITE_MANAGER
  if (!wm_class->main)
    {
      GMainLoop * loop = g_main_loop_new (NULL, FALSE);

      g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
		       mb_wm_main_context_gloop_xevent, wm->main_ctx, NULL);

      g_main_loop_run (loop);
      g_main_loop_unref (loop);
    }
  else
#endif
    {
      wm_class->main (wm);
    }
#endif
}

void
mb_wm_get_display_geometry (MBWindowManager  *wm,
			    MBGeometry       *geometry,
                            Bool              use_layout_flag)
{
  geometry->x = 0;
  geometry->y = 0;
  if (use_layout_flag && (wm->flags & MBWindowManagerFlagLayoutRotated))
    {
      geometry->width  = wm->xdpy_height;
      geometry->height = wm->xdpy_width;
    }
  else
    {
      geometry->width  = wm->xdpy_width;
      geometry->height = wm->xdpy_height;
    }
}

void
mb_wm_display_sync_queue (MBWindowManager* wm, MBWMSyncType sync)
{
  wm->sync_type |= sync;
}

static void
mb_wm_manage_preexisting_wins (MBWindowManager* wm)
{
   unsigned int      nwins, i;
   Window            foowin1, foowin2, *wins;
   XWindowAttributes attr;
   MBWindowManagerClass * wm_class =
     MB_WINDOW_MANAGER_CLASS (MB_WM_OBJECT_GET_CLASS (wm));

   if (!wm_class->client_new)
     return;

   XQueryTree(wm->xdpy, wm->root_win->xwindow,
	      &foowin1, &foowin2, &wins, &nwins);

   for (i = 0; i < nwins; i++)
     {
       XGetWindowAttributes(wm->xdpy, wins[i], &attr);

       if (
#if ! ENABLE_COMPOSITE
	   !attr.override_redirect &&
#endif
	   attr.map_state == IsViewable)
	 {
	   MBWMClientWindow      *win = NULL;
	   MBWindowManagerClient *client = NULL;

	   win = mb_wm_client_window_new (wm, wins[i]);

	   if (!win)
	     continue;

	   client = wm_class->client_new (wm, win);

	   if (client)
	     {
	       /*
		* When we realize the client, we reparent the application
		* window to the new frame, which generates an unmap event.
		* We need to skip it.  On the other hand, don't skip the
                * accompanying MapNotify this time.
		*/
	       client->skip_unmaps++;
               client->skip_maps--;
               MB_WM_DBG_SKIP_UNMAPS (client);

#if ENABLE_COMPOSITE
	       /*
	        * Register the new client with the composite manager before
	        * we call mb_wm_manage_client() -- this is necessary so that
	        * we can process map notification on the frame.
	        */
	       if (wm->comp_mgr && mb_wm_comp_mgr_enabled (wm->comp_mgr))
		 mb_wm_comp_mgr_register_client (wm->comp_mgr, client, False);
#endif
	       mb_wm_manage_client(wm, client, False);
	     }
	   else
	     mb_wm_object_unref (MB_WM_OBJECT (win));
	 }
     }

   wm->focus_after_stacking = True;

   XFree(wins);
}

static void
mb_wm_get_desktop_geometry (MBWindowManager *wm, MBGeometry * geom)
{
  MBWindowManagerClient *c;
  MBGeometry p_geom;
  MBWMClientLayoutHints hints;

  geom->x      = 0;
  geom->y      = 0;
  geom->width  = wm->xdpy_width;
  geom->height = wm->xdpy_height;

  if (mb_wm_stack_empty(wm))
    return;

  mb_wm_stack_enumerate(wm, c)
     {
       if (MB_WM_CLIENT_CLIENT_TYPE (c) != MBWMClientTypePanel ||
	   ((hints = mb_wm_client_get_layout_hints (c)) & LayoutPrefOverlaps))
	 continue;

       mb_wm_client_get_coverage (c, & p_geom);

       if (LayoutPrefReserveEdgeNorth & hints)
	 geom->y += p_geom.height;

       if (LayoutPrefReserveEdgeSouth & hints)
	 geom->height -= p_geom.height;

       if (LayoutPrefReserveEdgeWest & hints)
	 geom->x += p_geom.width;

       if (LayoutPrefReserveEdgeEast & hints)
	 geom->width -= p_geom.width;
     }
}

void
mb_wm_update_workarea (MBWindowManager *wm, const MBGeometry *geo)
{
  static CARD32 val[4];

  if (val[0] == geo->x && val[1] == geo->y
      && val[2] == geo->width && val[3] == geo->height)
    return;
  val[0] = geo->x;
  val[1] = geo->y;
  val[2] = geo->width;
  val[3] = geo->height;

  XChangeProperty(wm->xdpy, wm->root_win->xwindow,
                  wm->atoms[MBWM_ATOM_NET_WORKAREA],
		  XA_CARDINAL, 32, PropModeReplace,
		  (unsigned char *)val, 4);
}

static void
mb_wm_update_root_win_rectangles (MBWindowManager *wm)
{
  Display * dpy = wm->xdpy;
  Window    root = wm->root_win->xwindow;
  MBGeometry d_geom;
  CARD32 val[2];

  mb_wm_get_desktop_geometry (wm, &d_geom);
  mb_wm_update_workarea (wm, &d_geom);

  val[0] = wm->xdpy_width;
  val[1] = wm->xdpy_height;
  XChangeProperty(dpy, root, wm->atoms[MBWM_ATOM_NET_DESKTOP_GEOMETRY],
		  XA_CARDINAL, 32, PropModeReplace,
		  (unsigned char *)val, 2);
}

int
mb_wm_register_client_type (void)
{
  static int type_cnt = 0;
  return ++type_cnt;
}

static int
mb_wm_init_xdpy (MBWindowManager * wm, const char * display)
{
  if (!wm->xdpy)
    {
      wm->xdpy = XOpenDisplay(display ? display : getenv("DISPLAY"));

      if (!wm->xdpy)
	{
	  /* FIXME: Error codes */
	  mb_wm_util_fatal_error("Display connection failed");
	  return 0;
	}
    }

  wm->xscreen     = DefaultScreen(wm->xdpy);
  wm->xdpy_width  = DisplayWidth(wm->xdpy, wm->xscreen);
  wm->xdpy_height = DisplayHeight(wm->xdpy, wm->xscreen);

  return 1;
}

#if ENABLE_COMPOSITE
static Bool
mb_wm_init_comp_extensions (MBWindowManager *wm)
{
  int		                event_base, error_base;
  int		                damage_error;
  int		                xfixes_event, xfixes_error;

  if (!XCompositeQueryExtension (wm->xdpy, &event_base, &error_base))
    {
      fprintf (stderr, "matchbox: No composite extension\n");
      return False;
    }

  if (!XDamageQueryExtension (wm->xdpy,
			      &wm->damage_event_base, &damage_error))
    {
      fprintf (stderr, "matchbox: No damage extension\n");
      return False;
    }

  if (!XFixesQueryExtension (wm->xdpy, &xfixes_event, &xfixes_error))
    {
      fprintf (stderr, "matchbox: No XFixes extension\n");
      return False;
    }

  return True;
}
#endif

/*
 * This function must be called before the MBWindowManager object can be
 * used.
 */
void
mb_wm_init (MBWindowManager * wm)
{
  MBWindowManagerClass *wm_class;

  wm_class = (MBWindowManagerClass *) MB_WM_OBJECT_GET_CLASS (wm);

  /*
   * We just started. If the theme protect file exists we had a crash the
   * previous time we loaded the theme. Then this theme is broken, we need to
   * use the fallback theme.
   */
  if (mb_wm_theme_check_broken ()) {
    wm->theme_path = FALLBACK_THEME_PATH;
  }

  mb_wm_set_theme_from_path (wm, wm->theme_path);

  MBWM_ASSERT (wm_class->layout_new);

  mb_wm_set_layout (wm, wm_class->layout_new (wm));

#if ENABLE_COMPOSITE
  if (wm_class->comp_mgr_new && mb_wm_theme_use_compositing_mgr (wm->theme))
    mb_wm_compositing_on (wm);
#endif

  mb_wm_manage_preexisting_wins (wm);

  /*
   * Force an initial stack sync even when there are no managed windows (when
   * using compositor, this triggers call to MBWMCompMgr::restack(), allowing
   * the CM to set its house into order (i.e., a clutter-based compositor
   * might want to reorganize any auxiliar actors that it might have, depending
   * on whether the initial stack is empty or not.
   */
  wm->sync_type |= MBWMSyncStacking;
}


static int
mb_window_manager_init (MBWMObject *this, va_list vap)
{
  MBWindowManager      *wm = MB_WINDOW_MANAGER (this);
  MBWindowManagerClass *wm_class;
  MBWMObjectProp        prop;
  int                   argc = 0;
  char                **argv = NULL;

  prop = va_arg(vap, MBWMObjectProp);
  while (prop)
    {
      switch (prop)
	{
	case MBWMObjectPropArgc:
	  argc = va_arg(vap, int);
	  break;
	case MBWMObjectPropArgv:
	  argv = va_arg(vap, char **);
	  break;
	case MBWMObjectPropDpy:
	  wm->xdpy = va_arg(vap, Display *);
	  break;
	default:
	  MBWMO_PROP_EAT (vap, prop);
	}

      prop = va_arg(vap, MBWMObjectProp);
    }

  wm_class = (MBWindowManagerClass *) MB_WM_OBJECT_GET_CLASS (wm);

  wm->argv = argv;
  wm->argc = argc;

  if (argc && argv && wm_class->process_cmdline)
    wm_class->process_cmdline (wm);

  if (!mb_wm_init_xdpy (wm, NULL))
    return 0;

  if (getenv("MB_SYNC"))
    XSynchronize (wm->xdpy, True);

  mb_wm_debug_init (getenv("MB_DEBUG"));

  /* FIXME: Multiple screen handling */

  wm->xas_context = xas_context_new(wm->xdpy);

  mb_wm_atoms_init(wm);

#if ENABLE_COMPOSITE
  if (!mb_wm_init_comp_extensions (wm))
    return 0;
#endif

  wm->root_win = mb_wm_root_window_get (wm);

  mb_wm_update_root_win_rectangles (wm);

  wm->main_ctx = mb_wm_main_context_new (wm);

  mb_wm_main_context_x_event_handler_add (wm->main_ctx,
			     None,
			     MapRequest,
			     (MBWMXEventFunc)mb_wm_handle_map_request,
			     wm);

#if ENABLE_COMPOSITE
  mb_wm_main_context_x_event_handler_add (wm->main_ctx,
			     None,
			     MapNotify,
			     (MBWMXEventFunc)mb_wm_handle_map_notify,
			     wm);

  mb_wm_main_context_x_event_handler_add (wm->main_ctx,
			     None,
			     ConfigureNotify,
			     (MBWMXEventFunc)mb_wm_handle_composite_config_notify,
			     wm);
#endif

  mb_wm_main_context_x_event_handler_add (wm->main_ctx,
			     wm->root_win->xwindow,
			     ConfigureNotify,
			     (MBWMXEventFunc)mb_wm_handle_root_config_notify,
			     wm);

  mb_wm_main_context_x_event_handler_add (wm->main_ctx,
			     None,
			     ConfigureRequest,
			     (MBWMXEventFunc)mb_wm_handle_config_request,
			     wm);

  mb_wm_main_context_x_event_handler_add (wm->main_ctx,
  			     None,
			     PropertyNotify,
			     (MBWMXEventFunc)mb_wm_handle_property_notify,
			     wm);

  mb_wm_main_context_x_event_handler_add (wm->main_ctx,
			     None,
			     DestroyNotify,
			     (MBWMXEventFunc)mb_wm_handle_destroy_notify,
			     wm);

  mb_wm_main_context_x_event_handler_add (wm->main_ctx,
			     None,
			     UnmapNotify,
			     (MBWMXEventFunc)mb_wm_handle_unmap_notify,
			     wm);

  mb_wm_main_context_x_event_handler_add (wm->main_ctx,
			     None,
			     KeyPress,
			     (MBWMXEventFunc)mb_wm_handle_key_press,
			     wm);

  mb_wm_keys_init(wm);

  /* set the cursor invisible */
  {
    Pixmap pix = XCreatePixmap (wm->xdpy, wm->root_win->xwindow, 1, 1, 1);
    XColor col;
    Cursor blank_curs;

    memset (&col, 0, sizeof (col));
    blank_curs = XCreatePixmapCursor (wm->xdpy, pix, pix, &col, &col, 1, 1);
    XFreePixmap (wm->xdpy, pix);
    XDefineCursor(wm->xdpy, wm->root_win->xwindow, blank_curs);
  }

  return 1;
}

static void
mb_wm_cmdline_help (const char *arg0, Bool quit)
{
  FILE * f = stdout;
  const char * name;
  char * p = strrchr (arg0, '/');

  if (p)
    name = p+1;
  else
    name = arg0;

  fprintf (f, "\nThis is Matchbox Window Manager 2.\n");

  fprintf (f, "\nUsage: %s [options]\n\n", name);
  fprintf (f, "Options:\n");
  fprintf (f, "  -display display      : X display to connect to (alternatively, display\n"
              "                          can be specified using the DISPLAY environment\n"
              "                          variable).\n");
  fprintf (f, "  -sm-client-id id      : Session id.\n");
  fprintf (f, "  -theme-always-reload  : Reload theme even if it matches the currently\n"
              "                          loaded theme.\n");
  fprintf (f, "  -theme theme          : Load the specified theme\n");

  if (quit)
    exit (0);
}

static void
mb_wm_process_cmdline (MBWindowManager *wm)
{
  int i;
  char ** argv = wm->argv;
  int     argc = wm->argc;

  for (i = 1; i < argc; ++i)
    {
      if (!strcmp(argv[i], "-help") || !strcmp(argv[i], "--help"))
	{
	  mb_wm_cmdline_help (argv[0], True);
	}
      else if (!strcmp(argv[i], "-theme-always-reload"))
	{
	  wm->flags |= MBWindowManagerFlagAlwaysReloadTheme;
	}
      else if (i < argc - 1)
	{
	  /* These need to have a value after the name parameter */
	  if (!strcmp(argv[i], "-display"))
	    {
	      mb_wm_init_xdpy (wm, argv[++i]);
	    }
	  else if (!strcmp ("-sm-client-id", argv[i]))
	    {
	      wm->sm_client_id = argv[++i];
	    }
	  else if (!strcmp ("-theme", argv[i]))
	    {
	      wm->theme_path = argv[++i];
	    }
	}
    }

  /*
   * Anything below here needs a display conection
   */
  if (!wm->xdpy && !mb_wm_init_xdpy (wm, NULL))
    return;
}

void
mb_wm_activate_client (MBWindowManager * wm, MBWindowManagerClient *c)
{
  MBWindowManagerClass  *wm_klass;
  wm_klass = MB_WINDOW_MANAGER_CLASS (MB_WM_OBJECT_GET_CLASS (wm));

  MBWM_ASSERT (wm_klass->client_activate);

  wm_klass->client_activate (wm, c);
}


static Bool
mb_wm_activate_client_real (MBWindowManager * wm, MBWindowManagerClient *c)
{
  MBWMClientType c_type;
  Bool was_desktop;
  Bool is_desktop;
  MBWindowManagerClient * c_focus = c;
  MBWindowManagerClient * trans, *last_focused_transient;

  if (c == NULL)
    return False;

  c_type = MB_WM_CLIENT_CLIENT_TYPE (c);

  /*
   * Under no circumtances attempt to activate override windows; only call
   * show on them.
   */
  if (c_type == MBWMClientTypeOverride)
    {
      mb_wm_client_show (c);
      return True;
    }

  was_desktop = (wm->flags & MBWindowManagerFlagDesktop);

  /*
   * We are showing desktop if either the client is desktop, it is transient
   * for the desktop, or the last client was desktop, and the current is a
   * dialog or menu transiet for root.
   */
  is_desktop  = ((MB_WM_CLIENT_CLIENT_TYPE (c) == MBWMClientTypeDesktop) ||
		 ((trans = mb_wm_client_get_transient_for (c)) == c) ||
		 (was_desktop &&
		  !trans &&
		  (c_type & (MBWMClientTypeDialog|
			     MBWMClientTypeMenu|
			     MBWMClientTypeNote|
			     MBWMClientTypeOverride))));

  if (is_desktop)
    wm->flags |= MBWindowManagerFlagDesktop;
  else
    wm->flags &= ~MBWindowManagerFlagDesktop;

  mb_wm_client_show (c);

  last_focused_transient = mb_wm_client_get_last_focused_transient (c);

  if (last_focused_transient) {
    c_focus = last_focused_transient;
  }

  if (c->window->net_type ==
                wm->atoms[MBWM_ATOM_HILDON_WM_WINDOW_TYPE_ANIMATION_ACTOR] ||
      c->window->net_type ==
                wm->atoms[MBWM_ATOM_HILDON_WM_WINDOW_TYPE_HOME_APPLET])
    {
      g_debug ("Not focusing an animation actor or home applet.\n");
    }
  else
    {
      mb_wm_focus_client (wm, c_focus);
    }

  mb_wm_client_stack (c, 0);

  if (is_desktop != was_desktop)
    {
      CARD32 card = is_desktop ? 1 : 0;

      XChangeProperty(wm->xdpy, wm->root_win->xwindow,
		      wm->atoms[MBWM_ATOM_NET_SHOWING_DESKTOP],
		      XA_CARDINAL, 32, PropModeReplace,
		      (unsigned char *)&card, 1);
    }

  mb_wm_display_sync_queue (wm, MBWMSyncStacking | MBWMSyncVisibility);

  return True;
}

MBWindowManagerClient*
mb_wm_get_visible_main_client(MBWindowManager *wm)
{
  if ((wm->flags & MBWindowManagerFlagDesktop) && wm->desktop)
    return wm->desktop;

  return mb_wm_stack_get_highest_by_type (wm, MBWMClientTypeApp);
}

void __attribute__ ((visibility("hidden")))
mb_wm_handle_ping_reply (MBWindowManager * wm, MBWindowManagerClient *c)
{
  g_debug ("%s: entered", __FUNCTION__);
  if (c == NULL)
    return;

  if (mb_wm_client_ping_in_progress (c))
    {
      MBWindowManagerClass  *wm_klass;

      mb_wm_client_ping_stop (c);

      wm_klass = MB_WINDOW_MANAGER_CLASS (MB_WM_OBJECT_GET_CLASS (wm));

      if (wm_klass->client_responding)
	wm_klass->client_responding (wm, c);
    }
}

void __attribute__ ((visibility("hidden")))
mb_wm_handle_hang_client (MBWindowManager * wm, MBWindowManagerClient *c)
{
  MBWindowManagerClass  *wm_klass;

  if (c == NULL)
    return;

  wm_klass = MB_WINDOW_MANAGER_CLASS (MB_WM_OBJECT_GET_CLASS (wm));

  /* reference it in case it gets unreffed in the hang handlers */
  mb_wm_object_ref (MB_WM_OBJECT (c));
  if (!wm_klass->client_hang || !wm_klass->client_hang (wm, c))
    {
      /* if this client is still managed/mapped, refcount is > 1 */
      if (mb_wm_object_get_refcount (MB_WM_OBJECT (c)) > 1)
        mb_wm_client_shutdown (c);
    }

  if (mb_wm_client_ping_in_progress (c))
    /* hung client is now handled, no need for ping process */
    mb_wm_client_ping_stop (c);
  mb_wm_object_unref (MB_WM_OBJECT (c));
}

void
mb_wm_toggle_desktop (MBWindowManager * wm)
{
  Bool show = !(wm->flags & MBWindowManagerFlagDesktop);
  mb_wm_handle_show_desktop (wm, show);
}

void
mb_wm_handle_show_desktop (MBWindowManager * wm, Bool show)
{
  if (!wm->desktop)
    return;

  if (show)
    mb_wm_activate_client (wm, wm->desktop);
  else
    {
      MBWindowManagerClient * c =
	mb_wm_stack_get_highest_by_type (wm, MBWMClientTypeApp);

      if (c)
	mb_wm_activate_client (wm, c);
    }
}

static Bool
timestamp_predicate (Display *display,
                     XEvent  *xevent,
                     XPointer arg)
{
  MBWindowManager *wm = (MBWindowManager *)arg;
  Window xwindow = wm->root_win->xwindow;

  if (xevent->type == PropertyNotify &&
      xevent->xproperty.window == xwindow &&
      xevent->xproperty.atom == wm->atoms[MBWM_ATOM_NET_WORKAREA])
    return True;

  return False;
}

Time
mb_wm_get_server_time (MBWindowManager *wm)
{
  XEvent xevent;
  long data = 0;

  /* zero-length append to get timestamp in the PropertyNotify */
  XChangeProperty (wm->xdpy, wm->root_win->xwindow,
                   wm->atoms[MBWM_ATOM_NET_WORKAREA],
                   XA_CARDINAL, 32, PropModeAppend,
                   (unsigned char*)&data, 0);

  XIfEvent (wm->xdpy, &xevent, timestamp_predicate, (XPointer)wm);

  return xevent.xproperty.time;
}


static void
mb_wm_set_layout (MBWindowManager *wm, MBWMLayout *layout)
{
  wm->layout = layout;
  wm->sync_type |= (MBWMSyncGeometry | MBWMSyncVisibility);
}

static void
mb_wm_focus_client (MBWindowManager *wm, MBWindowManagerClient *client)
{
  MBWindowManagerClient *last_focused_transient;

  last_focused_transient = mb_wm_client_get_last_focused_transient (client);

  /*
   * If the last focused transient for this client is modal, we try to focus
   * the transient rather than the client itself
   */

  if (last_focused_transient &&
      mb_wm_client_is_modal (last_focused_transient))
    {
      client = last_focused_transient;
    }

  /* We refuse to focus a window if: */

  if (
      /* It's already focussed */
      wm->focused_client == client ||
      /* It doesn't want focus */
      !mb_wm_client_want_focus (client) ||
      /* It's the parent of the current modal focus-holder */
      (wm->focused_client &&
       client == mb_wm_client_get_transient_for (wm->focused_client) &&
       mb_wm_client_is_modal (wm->focused_client)) ||
      /* It isn't system-modal but the current focus-holder is,
         unless its parent is the current focus-holder
         (you're allowed to switch to the non-system-modal transients
          of a system-modal window) */
      (wm->modality_type == MBWMModalitySystem &&
       mb_wm_client_is_system_modal (wm->focused_client) &&
       !mb_wm_client_is_system_modal (client) &&
       wm->focused_client != mb_wm_client_get_transient_for (client))
      )
    return;

  if (!mb_wm_client_is_realized (client))
    {
      /* We need the window mapped before we can focus it, but do not
       * want to do a full-scale mb_wm_sync ():
       *
       * First We need to update layout, othewise the window will get frame
       * size of 0x0; then we can realize it, and do a display sync (i.e.,
       * map).
       */
      if (wm->layout)
	mb_wm_layout_update (wm->layout);

      mb_wm_client_realize (client);
      mb_wm_client_display_sync (client);

      /* focus what ever should be focused according to the stacking order,
       * because this window could be e.g. behind the current application */
      wm->focus_after_stacking = True;
      return;
    }

  mb_wm_client_focus (client);

  /*
   * If the window is already destroyed we will get a notification about it
   * later.
   */
  wm->focused_client = client;
  return;
}

void
mb_wm_unfocus_client (MBWindowManager *wm, MBWindowManagerClient *client)
{
  if (client != wm->focused_client)
    return;

  mb_wm_focus_client_as_stacked (wm, client);
}

void
mb_wm_cycle_apps (MBWindowManager *wm, Bool reverse)
{
  MBWindowManagerClient *old_top, *new_top;

  if (wm->flags & MBWindowManagerFlagDesktop)
    {
      mb_wm_handle_show_desktop (wm, False);
      return;
    }

  old_top = mb_wm_stack_get_highest_by_type (wm, MBWMClientTypeApp);

  new_top = mb_wm_stack_cycle_by_type(wm, MBWMClientTypeApp, reverse);

  if (new_top && old_top != new_top)
    {
#if ENABLE_COMPOSITE
      if (wm->comp_mgr && mb_wm_comp_mgr_enabled (wm->comp_mgr))
	mb_wm_comp_mgr_do_transition (wm->comp_mgr, old_top, new_top, reverse);
#endif
      mb_wm_activate_client (wm, new_top);
    }
}

static void
mb_wm_set_theme (MBWindowManager *wm, MBWMTheme * theme)
{
  if (!theme)
    return;

  XGrabServer(wm->xdpy);

  if (wm->theme)
    mb_wm_object_unref (MB_WM_OBJECT (wm->theme));

  wm->theme = theme;
  wm->sync_type |= (MBWMSyncGeometry | MBWMSyncVisibility | MBWMSyncDecor);

  /* When initializing the MBWindowManager object, the theme gets created
   * before the root window, so that the root window can interogate it,
   * so we can get here before the window is in place
   */
  if (wm->root_win)
    mb_wm_root_window_update_supported_props (wm->root_win);

  mb_wm_object_signal_emit (MB_WM_OBJECT (wm),
			    MBWindowManagerSignalThemeChange);

  XUngrabServer(wm->xdpy);
  XFlush(wm->xdpy);
}

void
mb_wm_set_theme_from_path (MBWindowManager *wm, const char *theme_path)
{
  MBWMTheme            *theme;
  MBWindowManagerClass *wm_class;

  wm_class = MB_WINDOW_MANAGER_CLASS (MB_WM_OBJECT_GET_CLASS (wm));

  if (wm->theme)
    {
      if (!(wm->flags & MBWindowManagerFlagAlwaysReloadTheme) &&
	  (wm->theme->path && theme_path &&
	   !strcmp (theme_path, wm->theme->path)))
	return;

      if (!wm->theme->path && !theme_path)
	return;
    }

  theme = wm_class->theme_new (wm, theme_path);

  mb_wm_set_theme (wm, theme);
}

void __attribute__ ((visibility("hidden")))
mb_wm_compositing_on (MBWindowManager * wm)
{
#if ENABLE_COMPOSITE
  MBWindowManagerClass  *wm_class =
    MB_WINDOW_MANAGER_CLASS (MB_WM_OBJECT_GET_CLASS (wm));

  if (!wm->comp_mgr && wm_class->comp_mgr_new)
    wm->comp_mgr = wm_class->comp_mgr_new (wm);

  if (wm->comp_mgr && !mb_wm_comp_mgr_enabled (wm->comp_mgr))
    {
      mb_wm_comp_mgr_turn_on (wm->comp_mgr);
      XSync (wm->xdpy, False);
    }
#endif
}


void __attribute__ ((visibility("hidden")))
mb_wm_compositing_off (MBWindowManager * wm)
{
#if ENABLE_COMPOSITE
  if (wm->comp_mgr && mb_wm_comp_mgr_enabled (wm->comp_mgr))
    mb_wm_comp_mgr_turn_off (wm->comp_mgr);
#endif
}

Bool __attribute__ ((visibility("hidden")))
mb_wm_compositing_enabled (MBWindowManager * wm)
{
#if ENABLE_COMPOSITE
  return mb_wm_comp_mgr_enabled (wm->comp_mgr);
#else
  return False;
#endif
}

MBWMModality
mb_wm_get_modality_type (MBWindowManager * wm)
{
  return wm->modality_type;
}

static void
mb_wm_set_n_desktops (MBWindowManager *wm, int n_desktops)
{
  CARD32 card32 = n_desktops;

  wm->n_desktops = n_desktops;

  XChangeProperty(wm->xdpy, wm->root_win->xwindow,
		  wm->atoms[MBWM_ATOM_NET_NUMBER_OF_DESKTOPS],
		  XA_CARDINAL, 32, PropModeReplace,
		  (unsigned char *)&card32, 1);

  /* FIXME -- deal with the case where the number is shrinking */
}


void __attribute__ ((visibility("hidden")))
mb_wm_select_desktop (MBWindowManager *wm, int desktop)
{
  CARD32                 card32 = desktop;
  MBWindowManagerClient *c;
  int                    old_desktop;

  if (desktop == wm->active_desktop)
    return;

  old_desktop = wm->active_desktop;

  wm->active_desktop = desktop;

  if (desktop >= wm->n_desktops)
    {
      mb_wm_set_n_desktops (wm, desktop + 1);
    }

  mb_wm_stack_enumerate (wm, c)
    mb_wm_client_desktop_change (c, desktop);

  XChangeProperty(wm->xdpy, wm->root_win->xwindow,
		  wm->atoms[MBWM_ATOM_NET_CURRENT_DESKTOP],
		  XA_CARDINAL, 32, PropModeReplace,
		  (unsigned char *)&card32, 1);

#if ENABLE_COMPOSITE
  if (mb_wm_compositing_enabled (wm))
    mb_wm_comp_mgr_select_desktop (wm->comp_mgr, desktop, old_desktop);
#endif
}

#define LEFT_GUTTER 8

void
mb_adjust_dialog_title_position (MBWindowManager *wm,
                                 int new_padding)
{
  MBWindowManagerClient *top;

  new_padding += LEFT_GUTTER;
  if (left_padding == new_padding)
    return;
  left_padding = new_padding;

  top = mb_wm_get_visible_main_client(wm);
  if (!top || top == wm->desktop)
    {
      g_debug ("No visible window-- bailing");
      return;
    }

  mb_wm_client_decor_mark_dirty (top);
  /* force redraw by pretending we changed the theme */
  mb_wm_object_signal_emit (MB_WM_OBJECT (wm),
                            MBWindowManagerSignalThemeChange);

}
