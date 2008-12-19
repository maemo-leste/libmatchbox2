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

  MBWM_MARK();

  client = MB_WM_CLIENT(this);

  wm = client->wmref;

  mb_wm_util_trap_x_errors();

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

  XSync(wm->xdpy, False);
  mb_wm_util_untrap_x_errors();

  parent = mb_wm_client_get_transient_for (MB_WM_CLIENT(this));

  if (parent)
    mb_wm_client_remove_transient (parent, MB_WM_CLIENT(this));
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
/*  attr.background_pixel  = BlackPixel(wm->xdpy, wm->xscreen);*/
/*  attr.background_pixmap = None;*/
  attr.event_mask = MBWMChildMask|MBWMButtonMask|ExposureMask;

  /* This should probably be called via rather than on new clien sync() ...? */
  /*
   * We only create a frame window if the client is decorated (decors are
   * constructed in the _init functions, so we can easily test if the frame
   * is needed or not).
   */
  if (client->decor)
    {
      if (client->xwin_frame == None)
	{
#if ENABLE_COMPOSITE
	  if (mb_wm_client_is_argb32 (client))
	    {
	      attr.colormap = client->window->colormap;

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
				CWOverrideRedirect|CWEventMask/*|CWBackPixel|
				CWBorderPixel*/|CWColormap,
				&attr);
	      mb_wm_rename_window (wm, client->xwin_frame, "alphaframe");
	    }
	  else
#endif
	    {
	      /* Decorated, with no frame, and
	       * non-ARGB32 or no compositor
	       */
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
        
      /*
       * Assume geometry sync will fix this up correctly
       * together with any decoration creation. Layout
       * manager will call this
       */
      XReparentWindow(wm->xdpy,
	              MB_WM_CLIENT_XWIN(client),
                      client->xwin_frame,
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
   * immediately bellow it, catching any input events that
   * fall outside of the system-modal client.
   */
  if (mb_wm_client_is_modal (client) &&
      !mb_wm_client_get_transient_for (client) &&
      mb_wm_get_modality_type (wm) == MBWMModalitySystem)
    {
      XSetWindowAttributes attr;
      attr.override_redirect = True;
      attr.event_mask        = MBWMChildMask|ButtonPressMask|
                               ExposureMask;

      client->xwin_modal_blocker =
	  XCreateWindow (wm->xdpy,
	                 wm->root_win->xwindow,
			 0, 0,
			 wm->xdpy_width,
			 wm->xdpy_height,
			 0,
			 CopyFromParent,
			 InputOnly,
			 CopyFromParent,
			 CWOverrideRedirect|CWEventMask,
			 &attr);
      mb_wm_rename_window (wm, client->xwin_modal_blocker, "modalblocker");
    }

  XSetWindowBorderWidth(wm->xdpy, MB_WM_CLIENT_XWIN(client), 0);

  XAddToSaveSet(wm->xdpy, MB_WM_CLIENT_XWIN(client));

  XSelectInput(wm->xdpy,
	       MB_WM_CLIENT_XWIN(client),
	       PropertyChangeMask);
}

static void
mb_wm_client_base_stack (MBWindowManagerClient *client,
			 int                    flags)
{
  /* Stack to highest/lowest possible possition in stack */
  MBWMList * t = mb_wm_client_get_transients (client);
  MBWMList * li;

  mb_wm_stack_move_top(client);

  mb_wm_util_list_foreach (t, (MBWMListForEachCB)mb_wm_client_stack,
			   (void*)flags);
  for (li = t; li; li = li->next)
    if (li->data == client->wmref->focused_client)
      {
        mb_wm_stack_move_top ((MBWindowManagerClient *)li->data);
        break;
      }

  mb_wm_util_list_free (t);
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
		  XReparentWindow(wm->xdpy, MB_WM_CLIENT_XWIN(client),
				  client->xwin_frame, 0, 0);
		  XMapWindow(wm->xdpy, client->xwin_frame);
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
		  client->skip_unmaps++;
		  XReparentWindow(wm->xdpy, MB_WM_CLIENT_XWIN(client),
				  wm->root_win->xwindow, 0, 0);
		  XUnmapWindow(wm->xdpy, client->xwin_frame);
		  XMapWindow(wm->xdpy, MB_WM_CLIENT_XWIN(client));
		  
		}
	    }
	  else
	    {
	      client->skip_unmaps++;
	      XReparentWindow(wm->xdpy, MB_WM_CLIENT_XWIN(client),
			      wm->root_win->xwindow,
			      client->window->geometry.x,
			      client->window->geometry.y);
	    }
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
	  XSetInputFocus (wm->xdpy, client->window->xwindow,
			  RevertToPointerRoot, CurrentTime);

	}
    }

  /*
   * Sync up any geometry 
   */
  if (mb_wm_client_needs_geometry_sync (client))
    {
      int x, y, w, h;
      CARD32 wgeom[4];

      mb_wm_util_trap_x_errors();

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

      /* FIXME: need flags to handle other stuff like configure events etc */

      /* Resize any decor */
      mb_wm_util_list_foreach(client->decor,
			      (MBWMListForEachCB)mb_wm_decor_handle_resize,
			      NULL);

      mb_wm_util_untrap_x_errors();
    }
  else if (mb_wm_client_needs_configure_request_ack (client))
    send_synthetic_configure_notify (client);


  /* Handle any mapping - should be visible state ? */

  if (mb_wm_client_needs_visibility_sync (client))
    {
      mb_wm_util_trap_x_errors();

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
	  if (client->xwin_frame)
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
      mb_wm_util_untrap_x_errors();
    }

  /* Paint any decor */

  mb_wm_util_trap_x_errors();

  if (mb_wm_client_needs_decor_sync (client))
    {
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
      mb_wm_util_list_foreach (client->decor,
			      (MBWMListForEachCB)mb_wm_decor_handle_repaint,
			      NULL);
    }

  mb_wm_util_untrap_x_errors();

  mb_wm_util_trap_x_errors();
  XSync(wm->xdpy, False);
  mb_wm_util_untrap_x_errors();
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
  MBWindowManager  *wm = client->wmref;
  Window            xwin = client->window->xwindow;

  if (!mb_wm_client_want_focus (client))
    return False;

  if (xwin == last_focused)
    return False;

  mb_wm_util_trap_x_errors ();

  XSetInputFocus(wm->xdpy, xwin, RevertToPointerRoot, CurrentTime);

  XChangeProperty(wm->xdpy, wm->root_win->xwindow,
		  wm->atoms[MBWM_ATOM_NET_ACTIVE_WINDOW],
		  XA_WINDOW, 32, PropModeReplace,
		  (unsigned char *)&xwin, 1);

  if (mb_wm_util_untrap_x_errors())
    return False;

  last_focused = xwin;

  return True;
}

void base_foo(void)
{
  ; /* nasty hack to workaround linking issues WTF...
     * use .la's rather than .a's ??
    */
}
