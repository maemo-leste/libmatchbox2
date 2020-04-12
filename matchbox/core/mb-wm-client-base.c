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
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include "mb-wm.h"
#include "mb-wm-theme.h"
#include "mb-wm-comp-mgr.h"

#include <X11/Xmd.h>

#if ENABLE_COMPOSITE
#include <X11/extensions/Xrender.h>
#endif

#ifdef HAVE_XEXT
#include <X11/extensions/shape.h>
#endif

static void
mb_wm_client_base_realize (MBWindowManagerClient *client);

static void
mb_wm_client_base_stack (MBWindowManagerClient *client,
			 int                    flags);
static void
mb_wm_client_base_show (MBWindowManagerClient *client);

static void
mb_wm_client_base_hide (MBWindowManagerClient *client);


static void
mb_wm_client_base_display_sync (MBWindowManagerClient *client);

static Bool
mb_wm_client_base_request_geometry (MBWindowManagerClient *client,
				    MBGeometry            *new_geometry,
				    MBWMClientReqGeomType  flags);

static Bool
mb_wm_client_base_focus (MBWindowManagerClient *client);

static void
mb_wm_client_base_class_init (MBWMObjectClass *klass)
{
  MBWindowManagerClientClass *client;

  MBWM_MARK();

  client = (MBWindowManagerClientClass *)klass;

  client->realize  = mb_wm_client_base_realize;
  client->geometry = mb_wm_client_base_request_geometry;
  client->stack    = mb_wm_client_base_stack;
  client->show     = mb_wm_client_base_show;
  client->hide     = mb_wm_client_base_hide;
  client->sync     = mb_wm_client_base_display_sync;
  client->focus    = mb_wm_client_base_focus;

#if MBWM_WANT_DEBUG
  klass->klass_name = "MBWMClientBase";
#endif
}

static void
mb_wm_client_base_destroy (MBWMObject *this)
{
  MBWindowManagerClient *parent;
  MBWindowManagerClient *client;
  MBWindowManager *wm;
  MBWMList *li, *next;

  MBWM_MARK();

  client = MB_WM_CLIENT(this);

  wm = client->wmref;

  mb_wm_util_async_trap_x_errors(wm->xdpy);

  if (client->xwin_frame)
    {
      XReparentWindow (wm->xdpy, MB_WM_CLIENT_XWIN(client),
		       wm->root_win->xwindow, 0, 0);

      XDestroyWindow (wm->xdpy, client->xwin_frame);
      client->xwin_frame = None;


    }

  if (client->xwin_modal_blocker)
    {
      XDestroyWindow (wm->xdpy, client->xwin_modal_blocker);
      client->xwin_modal_blocker = None;
    }

  mb_wm_util_async_untrap_x_errors();

  parent = mb_wm_client_get_transient_for (MB_WM_CLIENT(this));
  if (parent)
    mb_wm_client_remove_transient (parent, MB_WM_CLIENT(this));

  /* We have to make sure that the transients no longer refer to this
   * client, which is about the be destroyed. */
  for (li = client->transients; li; li = next)
    {
      MB_WM_CLIENT(li->data)->transient_for = NULL;
      next = li->next;
      free (li);
    }
}

static int
mb_wm_client_base_init (MBWMObject *this, va_list vap)
{
  return 1;
}

int
mb_wm_client_base_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (MBWMClientBaseClass),
	sizeof (MBWMClientBase),
	mb_wm_client_base_init,
	mb_wm_client_base_destroy,
	mb_wm_client_base_class_init
      };

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_CLIENT, 0);
    }

  return type;
}

static void
mb_wm_client_base_realize (MBWindowManagerClient *client)
{
  MBWindowManager *wm = client->wmref;

  XSetWindowAttributes attr;

  MBWM_ASSERT(client->window != NULL);

  /* create the frame window */

  attr.override_redirect = True;
  attr.background_pixel = attr.border_pixel = BlackPixel(wm->xdpy, wm->xscreen);
/*  attr.background_pixmap = None;*/
  attr.event_mask = MBWMChildMask|MBWMButtonMask|ExposureMask;

  /* This should probably be called via rather than on new clien sync() ...? */
  /*
   * We only create a frame window if the client is decorated (decors are
   * constructed in the _init functions, so we can easily test if the frame
   * is needed or not).
   *
   * FIXME "An exception is fullscreen windows, which should not have a frame,
   *        otherwise XRestackWindows() will be irritated."  Preexisting fs
   *        clients still don't work properly, and it breaks initially fs
   *        clients.
   */
  if (client->decor)
    {
      if (client->xwin_frame == None)
	{
          if (!client->frame_geometry.width || !client->frame_geometry.height)
            g_critical ("i'm [not] gonna create 0x0 frame window and play "
                        "hide and catch with you, have a good time");

#if ENABLE_COMPOSITE
	  if (mb_wm_client_is_argb32 (client))
	    {
	      attr.colormap = client->window->colormap;

              /* for some reason X wants us to define a BorderPixel */
	      client->xwin_frame
		= XCreateWindow(wm->xdpy, wm->root_win->xwindow,
				client->frame_geometry.x,
				client->frame_geometry.y,
				client->frame_geometry.width,
				client->frame_geometry.height,
				0,
				32,
				InputOutput,
				client->window->visual,
				CWOverrideRedirect|CWEventMask|/*CWBackPixel|*/
				CWBorderPixel|CWColormap,
				&attr);
	      mb_wm_rename_window (wm, client->xwin_frame, "alphaframe");
	    }
	  else
#endif
	    {
	      /* Decorated, with no frame, and non-ARGB32 or no compositor */
	      client->xwin_frame
		= XCreateWindow(wm->xdpy, wm->root_win->xwindow,
				client->frame_geometry.x,
				client->frame_geometry.y,
				client->frame_geometry.width,
				client->frame_geometry.height,
				0,
				CopyFromParent,
				CopyFromParent,
				CopyFromParent,
				CWOverrideRedirect|CWEventMask/*|CWBackPixel*/,
				&attr);
	      mb_wm_rename_window (wm, client->xwin_frame, "nonalphaframe");
	    }
        }

      g_debug("frame for window 0x%lx is 0x%lx",
              client->window->xwindow, client->xwin_frame);

#if ENABLE_COMPOSITE
      mb_wm_comp_mgr_client_maybe_redirect (wm->comp_mgr, client);
#endif

      /*
       * Assume geometry sync will fix this up correctly
       * together with any decoration creation. Layout
       * manager will call this
       */
      XReparentWindow(wm->xdpy,
	              MB_WM_CLIENT_XWIN(client),
                      (client->window->ewmh_state
                       & MBWMClientWindowEWMHStateFullscreen)
                        ?  client->wmref->root_win->xwindow
                        : client->xwin_frame,
                      0, 0);
    }
  else
    {
      /*
       * This is an undecorated client; we must reparent the window to our
       * root, otherwise we restacking of pre-existing windows might fail.
       */
      XReparentWindow(client->wmref->xdpy,
                      MB_WM_CLIENT_XWIN(client),
                      client->wmref->root_win->xwindow, 0, 0);
    }

  /*
   * If this is a system-modal client and the global setting
   * is to support system modal windows, we create a
   * fullscreen, input-only window that gets stacked
   * immediately below it, catching any input events that
   * fall outside of the system-modal client.
   */
  if ((mb_wm_client_is_modal (client) &&
      !mb_wm_client_get_transient_for (client) &&
      mb_wm_get_modality_type (wm) == MBWMModalitySystem) ||
      /* we create a blocker for application windows as well,
       * to block taps during non-fullscreen/fullscreen transition when
       * the window is unmapped for a short period of time */
      client->window->net_type ==
                wm->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_NORMAL])
    {
      int d;

      XSetWindowAttributes attr;
      attr.override_redirect = True;
      attr.event_mask        = MBWMChildMask|ButtonPressMask|
                               ExposureMask;

      /* Create a larger window because we might be rotated
       * while the dialog is running. */
      d = wm->xdpy_width > wm->xdpy_height ? wm->xdpy_width : wm->xdpy_height;
      client->xwin_modal_blocker =
	  XCreateWindow (wm->xdpy,
	                 wm->root_win->xwindow,
			 0, 0,
                         d, d,
			 0,
			 CopyFromParent,
			 InputOnly,
			 CopyFromParent,
			 CWOverrideRedirect|CWEventMask,
			 &attr);
      mb_wm_rename_window (wm, client->xwin_modal_blocker, "modalblocker");
    }

  XSetWindowBorderWidth(wm->xdpy, MB_WM_CLIENT_XWIN(client), 0);

  XSelectInput(wm->xdpy,
	       MB_WM_CLIENT_XWIN(client),
	       PropertyChangeMask);
  mb_wm_client_window_sync_properties (
		  client->window,
		  MBWM_WINDOW_PROP_TRANSIENCY
                  | MBWM_WINDOW_PROP_LIVE_BACKGROUND);

  XAddToSaveSet(wm->xdpy, MB_WM_CLIENT_XWIN(client));
}

/*
 * This window will move the client to the top of the stack with its transients.
 */
static void
mb_wm_client_move_to_top_recursive (
		MBWindowManagerClient   *client)
{
	MBWMList *transients;

	/*
	 * TODO: change this to a (yet to implement) function that uses the
	 * stacking layer properly.
	 */
	mb_wm_stack_move_top (client);

	transients = client->transients;
	while (transients) {
		mb_wm_client_move_to_top_recursive (
				(MBWindowManagerClient *) transients->data);
		transients = mb_wm_util_list_next (transients);
	}
}

static void
mb_wm_client_base_stack (MBWindowManagerClient *client,
                         int                    flags)
{
  MBWindowManagerClient *transient_for = client->transient_for;

  /*
   * If this is a transient we have to find the very first element of the
   * transient chain.
   */
  while (transient_for && transient_for->transient_for)
	  transient_for = transient_for->transient_for;
  /*
   * And then we are going to use the parent.
   */
  if (transient_for)
	  client = transient_for;

  /*
   * Move the window to the top with its transients.
   */
  mb_wm_client_move_to_top_recursive (client);
}

static void
mb_wm_client_base_show (MBWindowManagerClient *client)
{
  /* mark dirty somehow */

}

static void
mb_wm_client_base_hide (MBWindowManagerClient *client)
{

  /* mark dirty somehow */

}

static void
mb_wm_client_base_set_state_props (MBWindowManagerClient *c)
{
  unsigned long     flags = c->window->ewmh_state;
  Window            xwin  = c->window->xwindow;
  MBWindowManager  *wm    = c->wmref;
  Display          *xdpy  = wm->xdpy;
  CARD32            card32[2];
  Atom              ewmh_state [MBWMClientWindowEWHMStatesCount];
  int               ewmh_i = 0;

  card32[1] = None;

  if (mb_wm_client_is_mapped (c))
    card32[0] = NormalState;
  else
    card32[0] = IconicState;

  if (flags & MBWMClientWindowEWMHStateFullscreen)
    ewmh_state[ewmh_i++] = wm->atoms[MBWM_ATOM_NET_WM_STATE_FULLSCREEN];

  if (flags & MBWMClientWindowEWMHStateModal)
    ewmh_state[ewmh_i++] = wm->atoms[MBWM_ATOM_NET_WM_STATE_MODAL];

  if (flags & MBWMClientWindowEWMHStateSticky)
    ewmh_state[ewmh_i++] = wm->atoms[MBWM_ATOM_NET_WM_STATE_STICKY];

  if (flags & MBWMClientWindowEWMHStateMaximisedVert)
    ewmh_state[ewmh_i++] = wm->atoms[MBWM_ATOM_NET_WM_STATE_MAXIMIZED_VERT];
  if (flags & MBWMClientWindowEWMHStateMaximisedHorz)
    ewmh_state[ewmh_i++] = wm->atoms[MBWM_ATOM_NET_WM_STATE_MAXIMIZED_HORZ];
  if (flags & MBWMClientWindowEWMHStateShaded)
    ewmh_state[ewmh_i++] = wm->atoms[MBWM_ATOM_NET_WM_STATE_SHADED];

  if (flags & MBWMClientWindowEWMHStateSkipTaskbar)
    ewmh_state[ewmh_i++] = wm->atoms[MBWM_ATOM_NET_WM_STATE_SKIP_TASKBAR];

  if (flags & MBWMClientWindowEWMHStateSkipPager)
    ewmh_state[ewmh_i++] = wm->atoms[MBWM_ATOM_NET_WM_STATE_SKIP_PAGER];

  if (flags & MBWMClientWindowEWMHStateAbove)
    ewmh_state[ewmh_i++] = wm->atoms[MBWM_ATOM_NET_WM_STATE_BELOW];

  if (flags & MBWMClientWindowEWMHStateDemandsAttention)
    ewmh_state[ewmh_i++] = wm->atoms[MBWM_ATOM_NET_WM_STATE_DEMANDS_ATTENTION];
  if (flags & MBWMClientWindowEWMHStateHidden)
    ewmh_state[ewmh_i++] = wm->atoms[MBWM_ATOM_NET_WM_STATE_HIDDEN];


  XChangeProperty(xdpy, xwin, wm->atoms[MBWM_ATOM_WM_STATE],
		  wm->atoms[MBWM_ATOM_WM_STATE], 32, PropModeReplace,
		  (void *)&card32[0], 2);

  if (ewmh_i)
    XChangeProperty (xdpy, xwin, wm->atoms[MBWM_ATOM_NET_WM_STATE],
		     XA_ATOM, 32, PropModeReplace,
		     (void*) &ewmh_state[0], ewmh_i);
  else
    XDeleteProperty (xdpy, xwin, wm->atoms[MBWM_ATOM_NET_WM_STATE]);
}

static void
send_synthetic_configure_notify (MBWindowManagerClient *client)
{
  MBWindowManager *wm = client->wmref;
  XConfigureEvent ce;

  ce.type = ConfigureNotify;
  ce.event = MB_WM_CLIENT_XWIN(client);
  ce.window = MB_WM_CLIENT_XWIN(client);
  ce.x = client->window->geometry.x;
  ce.y = client->window->geometry.y;
  ce.width = client->window->geometry.width;
  ce.height = client->window->geometry.height;
  ce.border_width = 0;
  ce.above = None;
  ce.override_redirect = 0;

  XSendEvent(wm->xdpy, MB_WM_CLIENT_XWIN(client), False,
	     StructureNotifyMask, (XEvent *)&ce);
}

static void
move_resize_client_xwin (MBWindowManagerClient *client, int x, int y, int w, int h)
{
  MBWindowManager *wm = client->wmref;

  /* ICCCM says if you ignore a configure request or you respond
   * by only moving/re-stacking the window - without a size change,
   * then the WM must send a synthetic ConfigureNotify.
   *
   * NB: the above description assumes that a move/re-stack may be
   * done by the WM by moving the frame (whereby a regular
   * ConfigureNotify wouldn't be sent in direct response to the
   * request which I think is the real point)
   *
   * NB: It's assumed that no cleverness is going elsewhere
   * to optimise out calls to this function when the move/resize
   * is obviously not needed (e.g. when just moving the frame
   * of a client)
   */
  if (mb_wm_client_needs_configure_request_ack (client)
      && x == client->window->x_geometry.x
      && y == client->window->x_geometry.y
      && w == client->window->x_geometry.width
      && h == client->window->x_geometry.height)
    {
      send_synthetic_configure_notify (client);
    }
  else
    {
      MB_WM_DBG_MOVE_RESIZE ("client", MB_WM_CLIENT_XWIN(client),
                             (&(MBGeometry){x, y, w, h}));
      XMoveResizeWindow(wm->xdpy, MB_WM_CLIENT_XWIN(client),
			x, y, w, h);
      client->window->x_geometry.x = x;
      client->window->x_geometry.y = y;
      client->window->x_geometry.width = w;
      client->window->x_geometry.height = h;

#if ENABLE_COMPOSITE
      if (mb_wm_comp_mgr_enabled (wm->comp_mgr))
	{
	  mb_wm_comp_mgr_client_configure (client->cm_client);
	}
#endif
    }
}

static Bool
is_window_mapped (
    Display  *display,
    Window    window)
{
  XWindowAttributes attr;

  if (!XGetWindowAttributes (display, window, &attr))
    return False;

  return attr.map_state == IsViewable;
}

/* Set focus while taking into account the WM_TAKE_FOCUS protocol */
static Bool
mb_wm_client_set_focus (MBWindowManagerClient *client)
{
  MBWindowManager *wm = client->wmref;
  Window xwin = client->window->xwindow;
  gboolean success = True;

  //Unconditionaly call XSetInputFocus to avoid relying on clients calling it on themselves and _NET_ACTIVE_WINDOW working.
  XSetInputFocus(wm->xdpy, xwin, RevertToPointerRoot, CurrentTime);
  return success;
}

static void
mb_wm_client_base_display_sync (MBWindowManagerClient *client)
{
  MBWindowManager *wm = client->wmref;
  MBWMClientWindow * win = client->window;
  Bool fullscreen = (win->ewmh_state & MBWMClientWindowEWMHStateFullscreen);

  MBWM_MARK();

  /*
   * If we need to sync due to change in fullscreen state, we have reparent
   * the client window to the frame/root window
   */
  if (mb_wm_client_needs_fullscreen_sync (client))
    {
      if (mb_wm_client_is_mapped (client))
	{
	  if (client->xwin_frame)
	    {
	      if (!fullscreen)
		{
		  client->skip_unmaps++;
                  MB_WM_DBG_SKIP_UNMAPS (client);
		  XMapWindow(wm->xdpy, client->xwin_frame);
		  XReparentWindow(wm->xdpy, MB_WM_CLIENT_XWIN(client),
				  client->xwin_frame, 0, 0);
		  XMapSubwindows(wm->xdpy, client->xwin_frame);

		  /* The frame is very likely the correct dimensions (since the
		   * app geometry is pretty static), but the client window is
		   * not (it was fullscreened) -- we need to force
		   * recalculation of the window frame dimensions, so that it
		   * is correct when we apply it below.
		   */
		}
	      else
		{
		  if (is_window_mapped(wm->xdpy, MB_WM_CLIENT_XWIN(client)))
                    {
                      client->skip_unmaps++;
                      MB_WM_DBG_SKIP_UNMAPS (client);
                    }

		  XReparentWindow(wm->xdpy, MB_WM_CLIENT_XWIN(client),
				  wm->root_win->xwindow, 0, 0);
		  XUnmapWindow(wm->xdpy, client->xwin_frame);
		  XMapWindow(wm->xdpy, MB_WM_CLIENT_XWIN(client));

		}
	    }
	  else if (!client->window->undecorated)
	    { /* Undecorated windows are always parented at the root. */
	      client->skip_unmaps++;
              MB_WM_DBG_SKIP_UNMAPS (client);
	      XReparentWindow(wm->xdpy, MB_WM_CLIENT_XWIN(client),
			      wm->root_win->xwindow,
			      client->window->geometry.x,
			      client->window->geometry.y);
	    }
          /* What if the window has changed its undecoratedness hint?
           * We have never supported it, it seems. */
	}

      mb_wm_client_request_geometry (
	  client,
	  &client->frame_geometry,
	  MBWMClientReqGeomForced);

      if (wm->focused_client == client)
	{
	  /*
	   * If we are the currently focused client,
	   * we need to reset the focus to RevertToPointerRoot, since the
	   * focus was lost during the implicit unmap.
	   */
          mb_wm_client_set_focus (client);
	}
    }

  /*
   * Sync up any geometry.  Don't touch hidden (and not-yet-shown) clients.
   */
  if (mb_wm_client_is_mapped (client) && mb_wm_client_needs_geometry_sync (client))
    {
      int x, y, w, h;
      CARD32 wgeom[4];

      mb_wm_util_async_trap_x_errors(wm->xdpy);

      if (fullscreen || !client->xwin_frame)
	{
	  x = client->window->geometry.x;
	  y = client->window->geometry.y;
	  w = client->window->geometry.width;
	  h = client->window->geometry.height;

	  move_resize_client_xwin (client, x, y, w, h);

	  wgeom[0] = 0;
	  wgeom[1] = 0;
	  wgeom[2] = 0;
	  wgeom[3] = 0;
	}
      else
	{
          MB_WM_DBG_MOVE_RESIZE ("frame", client->xwin_frame,
                                 &client->frame_geometry);
	  XMoveResizeWindow(wm->xdpy,
			    client->xwin_frame,
			    client->frame_geometry.x,
			    client->frame_geometry.y,
			    client->frame_geometry.width,
			    client->frame_geometry.height);

	  /* FIXME: Call XConfigureWindow(w->dpy, e->window,
	   *        value_mask,&xwc); here instead as can set border
	   *        width = 0.
	   */
	  x = client->window->geometry.x - client->frame_geometry.x;
	  y = client->window->geometry.y - client->frame_geometry.y;
	  w = client->window->geometry.width;
	  h = client->window->geometry.height;

	  move_resize_client_xwin (client, x, y, w, h);

	  wgeom[0] = x;
	  wgeom[1] = client->frame_geometry.width - w - x;
	  wgeom[2] = y;
	  wgeom[3] = client->frame_geometry.height - h - y;
	}

      XChangeProperty(wm->xdpy,
		      MB_WM_CLIENT_XWIN(client),
		      wm->atoms[MBWM_ATOM_NET_FRAME_EXTENTS],
		      XA_CARDINAL, 32, PropModeReplace,
		      (unsigned char *)&wgeom[0], 4);

      mb_wm_util_async_untrap_x_errors();
      /* FIXME: need flags to handle other stuff like configure events etc */

      /* Resize any decor */
      mb_wm_util_list_foreach(client->decor,
			      (MBWMListForEachCB)mb_wm_decor_handle_resize,
			      NULL);
    }
  else if (mb_wm_client_needs_configure_request_ack (client))
    send_synthetic_configure_notify (client);


  /* Handle any mapping - should be visible state ? */

  if (mb_wm_client_needs_visibility_sync (client))
    {
      mb_wm_util_async_trap_x_errors(wm->xdpy);

      if (mb_wm_client_is_mapped (client))
	{
	  if (client->xwin_frame)
	    {
	      if (!fullscreen)
		{
		  XMapWindow (wm->xdpy, client->xwin_frame);
		  XMapSubwindows (wm->xdpy, client->xwin_frame);
		}
	      else
		{
		  XUnmapWindow (wm->xdpy, client->xwin_frame);
		  XMapWindow (wm->xdpy, MB_WM_CLIENT_XWIN(client));
		}
	    }
	  else
	    {
	      XMapWindow (wm->xdpy, MB_WM_CLIENT_XWIN(client));
	    }

	  if (client->xwin_modal_blocker)
	    XMapWindow (wm->xdpy, client->xwin_modal_blocker);
	}
      else
	{
	  if (!fullscreen)
	    {
	      XUnmapWindow (wm->xdpy, client->xwin_frame);
	      XUnmapSubwindows (wm->xdpy, client->xwin_frame);
	    }
	  else
	    XUnmapWindow (wm->xdpy, MB_WM_CLIENT_XWIN(client));


	  if (client->xwin_modal_blocker)
	    XUnmapWindow (wm->xdpy, client->xwin_modal_blocker);
	}

      mb_wm_client_base_set_state_props (client);
      mb_wm_util_async_untrap_x_errors();
    }

  /* Paint any decor */
  if (mb_wm_client_needs_decor_sync (client))
    {
      MBGeometry area;

      mb_wm_util_async_trap_x_errors(wm->xdpy);
      /*
       * First, we set the base shape mask, if needed, so that individual
       * decors can add themselves to it.
       */
#ifdef HAVE_XEXT
      if (mb_wm_theme_is_client_shaped (wm->theme, client))
	{
	  XRectangle rects[1];

	  rects[0].x = client->window->geometry.x - client->frame_geometry.x;
	  rects[0].y = client->window->geometry.y - client->frame_geometry.y;
	  rects[0].width  = client->window->geometry.width;
	  rects[0].height = client->window->geometry.height;

	  XShapeCombineRectangles (wm->xdpy, client->xwin_frame,
				   ShapeBounding,
				   0, 0, rects, 1, ShapeSet, 0 );
	}
#endif

      /* This is used to tell gtk where to place its precious comboboxes. */
      area = client->window->geometry;
      area.x -= client->frame_geometry.x;
      area.y -= client->frame_geometry.y;
      mb_wm_update_workarea (wm, &area);

#if 0
      /*
       * I don't think this part is doing any good...
       */
      if (fullscreen)
	{
	  if (client->xwin_frame)
	    XUnmapWindow(wm->xdpy, client->xwin_frame);
	}
      else
	{
	  if (client->xwin_frame)
	    XMapWindow(wm->xdpy, client->xwin_frame);
	}
#endif
      mb_wm_util_async_untrap_x_errors();

      mb_wm_util_list_foreach (client->decor,
			      (MBWMListForEachCB)mb_wm_decor_handle_repaint,
			      NULL);
    }


}

/* Note request geometry always called by layout manager */
static Bool
mb_wm_client_base_request_geometry (MBWindowManagerClient *client,
				    MBGeometry            *new_geometry,
				    MBWMClientReqGeomType  flags)
{
  /*
   *  flags are
   *
   *  MBReqGeomDontCommit
   *  MBReqGeomIsViaConfigureReq
   *  MBReqGeomIsViaUserAction
   *  MBReqGeomIsViaLayoutManager
   *  MBReqGeomForced
   *
  */

  /* Dont actually change anything - mb_wm_core_sync() should do that
   * but mark dirty and 'queue any extra info like configure req'.
  */

  if (flags & MBWMClientReqGeomIsViaLayoutManager)
    {
      MBWM_DBG("called with 'MBWMClientReqGeomIsViaLayoutManager' %ix%i+%i+%i",
	       new_geometry->width,
	       new_geometry->height,
	       new_geometry->x,
	       new_geometry->y);

      client->frame_geometry.x      = new_geometry->x;
      client->frame_geometry.y      = new_geometry->y;
      client->frame_geometry.width  = new_geometry->width;
      client->frame_geometry.height = new_geometry->height;

      client->window->geometry.x = client->frame_geometry.x + 4;
      client->window->geometry.y = client->frame_geometry.y + 4;
      client->window->geometry.width  = client->frame_geometry.width - 8;
      client->window->geometry.height = client->frame_geometry.height - 8;

      mb_wm_client_geometry_mark_dirty (client);

      return True; /* Geometry accepted */
    }

  return True;
}

static Bool
mb_wm_client_base_focus (MBWindowManagerClient *client)
{
  static Window     last_focused = None;
  Window xwin = client->window->xwindow;
  MBWindowManager  *wm = client->wmref;
  gboolean success;

  if (!mb_wm_client_want_focus (client))
    return False;

  if (xwin == last_focused)
    return False;

  if (client->window->net_type==
      wm->atoms[MBWM_ATOM_HILDON_WM_WINDOW_TYPE_ANIMATION_ACTOR])
    {
      g_debug ("Not focusing an animation actor.\n");
      return False;
    }

  success = mb_wm_client_set_focus (client);

  XChangeProperty(wm->xdpy, wm->root_win->xwindow,
		  wm->atoms[MBWM_ATOM_NET_ACTIVE_WINDOW],
		  XA_WINDOW, 32, PropModeReplace,
		  (unsigned char *)&xwin, 1);

  if (!success)
    return False;

  last_focused = xwin;

  return True;
}
