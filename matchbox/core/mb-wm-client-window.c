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

/* Via Xatomtype.h */
#define NumPropWMHintsElements 9 /* number of elements in this structure */

enum {
  COOKIE_WIN_TYPE = 1,
  COOKIE_WIN_ATTR,
  COOKIE_WIN_GEOM,
  COOKIE_WIN_NAME,
  COOKIE_WIN_NAME_UTF8,
  COOKIE_WIN_NAME_UTF8_XML,
  COOKIE_WIN_SIZE_HINTS,
  COOKIE_WIN_WM_HINTS,
  COOKIE_WIN_TRANSIENCY,
  COOKIE_WIN_PROTOS,
  COOKIE_WIN_MACHINE,
  COOKIE_WIN_PID,
  COOKIE_WIN_ICON,
  COOKIE_WIN_ACTIONS,
  COOKIE_WIN_USER_TIME,
  COOKIE_WIN_CM_TRANSLUCENCY,
  COOKIE_WIN_NET_STATE,
  COOKIE_WIN_MWM_HINTS,
  COOKIE_WIN_HILDON_STACKING,
  COOKIE_WIN_HILDON_TYPE,
  COOKIE_WIN_PORTRAIT_SUPPORT,
  COOKIE_WIN_PORTRAIT_REQUEST,
  COOKIE_WIN_NO_TRANSITIONS,
  COOKIE_WIN_LIVE_BACKGROUND,

  N_COOKIES
};

#define PROP_MOTIF_WM_HINTS_ELEMENTS    5
#define MWM_HINTS_DECORATIONS   (1L << 1)

#define MWM_DECOR_BORDER              (1L << 1)
#define MWM_DECOR_RESIZEH             (1L << 2)
#define MWM_DECOR_TITLE               (1L << 3)
#define MWM_DECOR_MENU                (1L << 4)
#define MWM_DECOR_MINIMIZE            (1L << 5)
#define MWM_DECOR_MAXIMIZE            (1L << 6)

static void
mb_wm_client_window_class_init (MBWMObjectClass *klass)
{
/*  MBWMClientWindowClass *rw_class; */

  MBWM_MARK();

/*  rw_class = (MBWMClientWindowClass *)klass; */

#if MBWM_WANT_DEBUG
  klass->klass_name = "MBWMClientWindow";
#endif
}

static void
mb_wm_client_window_destroy (MBWMObject *this)
{
  MBWMClientWindow * win = MB_WM_CLIENT_WINDOW (this);
  MBWMList         * l   = win->icons;

  if (win->name)
    XFree (win->name);

  if (win->machine)
    XFree (win->machine);

  while (l)
    {
      mb_wm_rgba_icon_free ((MBWMRgbaIcon *)l->data);
      l = l->next;
    }

  memset (win, 0, sizeof (*win));
}

static int
mb_wm_client_window_init (MBWMObject *this, va_list vap)
{
  MBWMClientWindow *win = MB_WM_CLIENT_WINDOW (this);
  MBWMObjectProp    prop;
  MBWindowManager  *wm = NULL;
  Window            xwin = None;

  prop = va_arg(vap, MBWMObjectProp);
  while (prop)
    {
      switch (prop)
	{
	case MBWMObjectPropWm:
	  wm = va_arg(vap, MBWindowManager *);
	  break;
	case MBWMObjectPropXwindow:
	  xwin = va_arg(vap, Window);
	  break;
	default:
	  MBWMO_PROP_EAT (vap, prop);
	}

      prop = va_arg(vap, MBWMObjectProp);
    }

  if (!wm || !xwin)
    {
      g_critical ("--- error, no wm or xwin ---");
      return 0;
    }

  win->xwindow = xwin;
  win->wm = wm;
  win->portrait_supported = win->portrait_requested = -1;

  /* TODO: handle properties after discovering them. E.g. fullscreen.
   * See NB#97342 */
  return mb_wm_client_window_sync_properties (win, MBWM_WINDOW_PROP_ALL);
}

int
mb_wm_client_window_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (MBWMClientWindowClass),
	sizeof (MBWMClientWindow),
	mb_wm_client_window_init,
	mb_wm_client_window_destroy,
	mb_wm_client_window_class_init
      };

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_OBJECT, 0);
    }

  return type;
}

MBWMClientWindow*
mb_wm_client_window_new (MBWindowManager *wm, Window xwin)
{
  MBWMClientWindow *win;

  win = MB_WM_CLIENT_WINDOW (mb_wm_object_new (MB_WM_TYPE_CLIENT_WINDOW,
					       MBWMObjectPropWm,      wm,
					       MBWMObjectPropXwindow, xwin,
					       NULL));
  return win;
}

Bool
mb_wm_client_window_sync_properties ( MBWMClientWindow *win,
				     unsigned long     props_req)
{
  MBWMCookie       cookies[N_COOKIES] = {0};
  MBWindowManager *wm = win->wm;
  Atom             actual_type_return;
  unsigned char   *result_atom = NULL;
  int              actual_format_return;
  unsigned long    nitems_return;
  unsigned long    bytes_after_return;
  unsigned int     foo;
  int              x_error_code = Success;
  Window           xwin;
  int              changes = 0;
  Bool             abort_exit = False;

  MBWMClientWindowAttributes *xwin_attr = NULL;

  xwin = win->xwindow;

  if (props_req & MBWM_WINDOW_PROP_WIN_TYPE)
    cookies[COOKIE_WIN_TYPE]
      = mb_wm_property_atom_req(wm, xwin,
				wm->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE]);

  if (props_req & MBWM_WINDOW_PROP_WIN_HILDON_TYPE)
    cookies[COOKIE_WIN_HILDON_TYPE]
      = mb_wm_property_atom_req(wm, xwin,
				wm->atoms[MBWM_ATOM_HILDON_WM_WINDOW_TYPE]);

  if (props_req & MBWM_WINDOW_PROP_NET_STATE)
    cookies[COOKIE_WIN_NET_STATE]
      = mb_wm_property_atom_req(wm, xwin,
				wm->atoms[MBWM_ATOM_NET_WM_STATE]);

  if (props_req & MBWM_WINDOW_PROP_ATTR)
    cookies[COOKIE_WIN_ATTR]
      = mb_wm_xwin_get_attributes (wm, xwin);

  if (props_req & MBWM_WINDOW_PROP_GEOMETRY)
    cookies[COOKIE_WIN_GEOM]
      = mb_wm_xwin_get_geometry (wm, (Drawable)xwin);

  if (props_req & MBWM_WINDOW_PROP_NAME)
    {
      cookies[COOKIE_WIN_NAME]
	= mb_wm_property_req (wm,
			      xwin,
			      wm->atoms[MBWM_ATOM_WM_NAME],
			      0,
			      2048L,
			      False,
			      XA_STRING);

      cookies[COOKIE_WIN_NAME_UTF8] =
	mb_wm_property_utf8_req(wm, xwin,
				wm->atoms[MBWM_ATOM_NET_WM_NAME]);

      cookies[COOKIE_WIN_NAME_UTF8_XML] =
	mb_wm_property_utf8_req(wm, xwin,
				wm->atoms[MBWM_ATOM_HILDON_WM_NAME]);
    }

  if (props_req & MBWM_WINDOW_PROP_WM_HINTS)
    {
      cookies[COOKIE_WIN_WM_HINTS]
	= mb_wm_property_req (wm,
			      xwin,
			      wm->atoms[MBWM_ATOM_WM_HINTS],
			      0,
			      1024L,
			      False,
			      XA_WM_HINTS);
    }

  if (props_req & MBWM_WINDOW_PROP_MWM_HINTS)
    {
      cookies[COOKIE_WIN_MWM_HINTS]
	= mb_wm_property_req (wm,
			      xwin,
			      wm->atoms[MBWM_ATOM_MOTIF_WM_HINTS],
			      0,
			      PROP_MOTIF_WM_HINTS_ELEMENTS,
			      False,
			      wm->atoms[MBWM_ATOM_MOTIF_WM_HINTS]);
    }

  if (props_req & MBWM_WINDOW_PROP_TRANSIENCY)
    {
      cookies[COOKIE_WIN_TRANSIENCY]
	= mb_wm_property_req (wm,
			      xwin,
			      wm->atoms[MBWM_ATOM_WM_TRANSIENT_FOR],
			      0,
			      1L,
			      False,
			      XA_WINDOW);
    }

  if (props_req & MBWM_WINDOW_PROP_PROTOS)
    {
      cookies[COOKIE_WIN_PROTOS]
	= mb_wm_property_atom_req(wm, xwin,
				  wm->atoms[MBWM_ATOM_WM_PROTOCOLS]);
    }

  if (props_req & MBWM_WINDOW_PROP_CLIENT_MACHINE)
    {
      cookies[COOKIE_WIN_MACHINE]
	= mb_wm_property_req (wm,
			      xwin,
			      wm->atoms[MBWM_ATOM_WM_CLIENT_MACHINE],
			      0,
			      2048L,
			      False,
			      XA_STRING);
    }

  if (props_req & MBWM_WINDOW_PROP_LIVE_BACKGROUND)
    cookies[COOKIE_WIN_LIVE_BACKGROUND]
	= mb_wm_property_req (wm,
			      xwin,
		      wm->atoms[MBWM_ATOM_HILDON_LIVE_DESKTOP_BACKGROUND],
			      0,
			      1L,
			      False,
			      XA_INTEGER);

  if (props_req & MBWM_WINDOW_PROP_NET_PID)
    {
      cookies[COOKIE_WIN_PID]
	= mb_wm_property_cardinal_req (wm,
				       xwin,
				       wm->atoms[MBWM_ATOM_NET_WM_PID]);
    }

  if (props_req & MBWM_WINDOW_PROP_NO_TRANSITIONS)
    {
      cookies[COOKIE_WIN_NO_TRANSITIONS]
	= mb_wm_property_cardinal_req (wm, xwin,
	        wm->atoms[MBWM_ATOM_HILDON_WM_ACTION_NO_TRANSITIONS]);
    }

  if (props_req & MBWM_WINDOW_PROP_NET_USER_TIME)
    {
      cookies[COOKIE_WIN_USER_TIME]
	= mb_wm_property_cardinal_req (wm,
				       xwin,
				       wm->atoms[MBWM_ATOM_NET_WM_USER_TIME]);
    }

  if (props_req & MBWM_WINDOW_PROP_CM_TRANSLUCENCY)
    {
      cookies[COOKIE_WIN_CM_TRANSLUCENCY]
	= mb_wm_property_cardinal_req (wm,
				       xwin,
				       wm->atoms[MBWM_ATOM_CM_TRANSLUCENCY]);
    }

  if (props_req & MBWM_WINDOW_PROP_HILDON_STACKING)
    {
      cookies[COOKIE_WIN_HILDON_STACKING]
	= mb_wm_property_cardinal_req (wm, xwin,
	  	wm->atoms[MBWM_ATOM_HILDON_STACKING_LAYER]);
    }

  if (props_req & MBWM_WINDOW_PROP_PORTRAIT)
    {
      cookies[COOKIE_WIN_PORTRAIT_SUPPORT]
	= mb_wm_property_cardinal_req (wm, xwin,
	  	wm->atoms[MBWM_ATOM_HILDON_PORTRAIT_MODE_SUPPORT]);
      cookies[COOKIE_WIN_PORTRAIT_REQUEST]
	= mb_wm_property_cardinal_req (wm, xwin,
	  	wm->atoms[MBWM_ATOM_HILDON_PORTRAIT_MODE_REQUEST]);
    }

  {
    /* FIXME: toggling 'offline' mode in power menu can cause X error here */
    /* bundle all pending requests to server and wait for replys.
     * Errors will be caught by the new error handler. Note that removing this
     * absolutely kills hildon-desktop. It appears that the property specifying
     * window type doesn't get read and everything gets mapped as an app.  */
    XSync(wm->xdpy, False);
  }

  if (props_req & MBWM_WINDOW_PROP_TRANSIENCY)
    {
      Window *trans_win = NULL;

      trans_win
	= mb_wm_property_get_reply_and_validate (wm,
						 cookies[COOKIE_WIN_TRANSIENCY],
						 MBWM_ATOM_WM_TRANSIENT_FOR,
						 32,
						 1,
						 NULL,
						 &x_error_code);
      cookies[COOKIE_WIN_TRANSIENCY] = 0;

      if (x_error_code == BadWindow)
        {
          if (trans_win)
            XFree(trans_win);
          goto badwindow_error;
        }

      /* FIXME: we cannot really change transiency lists after mapping the
       * window because the compositor cannot handle that. Maybe we should
       * have a virtual function for window classes for change in transiency,
       * so that the compositor could implement it and handle it. */
      if (trans_win)
	{
	  MBWM_DBG("@@@ Window transient for %lx @@@", *trans_win);

	  if (*trans_win != win->xwin_transient_for)
	    {
	      changes |= MBWM_WINDOW_PROP_TRANSIENCY;

	      win->xwin_transient_for = *trans_win;
	    }

	  XFree(trans_win);
	}
      else
        {
          MBWM_DBG("@@@ Window transient for nothing @@@");
        }
    }

  if (props_req & MBWM_WINDOW_PROP_ATTR)
    {
      xwin_attr = mb_wm_xwin_get_attributes_reply (wm,
						   cookies[COOKIE_WIN_ATTR],
						   &x_error_code);
      cookies[COOKIE_WIN_ATTR] = 0;

      if (!xwin_attr || x_error_code)
	{
	  MBWM_DBG("### Warning Get Attr Failed ( %i ) ###", x_error_code);

          if (x_error_code == BadWindow)
            {
              if (xwin_attr)
                XFree (xwin_attr);
              goto badwindow_error;
            }

	  goto abort;
	}

      win->visual            = xwin_attr->visual;
      win->colormap          = xwin_attr->colormap;
      win->gravity           = xwin_attr->win_gravity;
      win->override_redirect = xwin_attr->override_redirect;
      win->window_class      = xwin_attr->class;

      XFree (xwin_attr);
      xwin_attr = NULL;
    }

  if (props_req & MBWM_WINDOW_PROP_WIN_TYPE)
    {
      mb_wm_property_reply (wm,
			    cookies[COOKIE_WIN_TYPE],
			    &actual_type_return,
			    &actual_format_return,
			    &nitems_return,
			    &bytes_after_return,
			    &result_atom,
			    &x_error_code);

      cookies[COOKIE_WIN_TYPE] = 0;

      if (x_error_code
	  || actual_type_return != XA_ATOM
	  || actual_format_return != 32
	  || nitems_return != 1
	  || result_atom == NULL
	  )
	{
          if (!win->override_redirect)
            g_debug ("%s: ### Warning net type prop failed for %lx ###",
                     __FUNCTION__, xwin);
	  if (x_error_code == BadWindow)
	    goto badwindow_error;

          /* FDO wm-spec says that managed windows without type and without
           * TRANSIENT_FOR must be taken as _NET_WM_WINDOW_TYPE_NORMAL. Also,
           * override-redirect windows without type must be taken as such. */
          if (!win->override_redirect && win->xwin_transient_for == None)
            {
	      g_debug ("%s: managed, non-transient window without a type",
                       __FUNCTION__);
	      win->net_type = wm->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_NORMAL];
            }
          else if (win->override_redirect)
            {
	      win->net_type = wm->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_NORMAL];
            }
	}
      else
	{
	  if (win->net_type != ((Atom *)result_atom)[0])
	    changes |= MBWM_WINDOW_PROP_WIN_TYPE;

          if (((Atom*)result_atom)[0]
                          == wm->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_DND])
            {
              g_warning ("%s: treat DND window as normal\n", __func__);
              win->net_type = wm->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_NORMAL];
            }
          else
	    win->net_type = ((Atom*)result_atom)[0];
	}

      if (result_atom)
	XFree(result_atom);

      result_atom = NULL;
    }

  if (props_req & MBWM_WINDOW_PROP_WIN_HILDON_TYPE)
    {
      mb_wm_property_reply (wm,
			    cookies[COOKIE_WIN_HILDON_TYPE],
			    &actual_type_return,
			    &actual_format_return,
			    &nitems_return,
			    &bytes_after_return,
			    &result_atom,
			    &x_error_code);

      cookies[COOKIE_WIN_HILDON_TYPE] = 0;

      if (x_error_code
	  || actual_type_return != XA_ATOM
	  || actual_format_return != 32
	  || nitems_return != 1
	  || result_atom == NULL
	  )
	{
	  if (x_error_code == BadWindow)
	    goto badwindow_error;
	}
      else
	{
	  if (win->hildon_type != ((Atom *)result_atom)[0])
	    changes |= MBWM_WINDOW_PROP_WIN_HILDON_TYPE;

	  win->hildon_type = ((Atom*)result_atom)[0];
	}

      if (result_atom)
	XFree(result_atom);

      result_atom = NULL;
    }

  if (props_req & MBWM_WINDOW_PROP_NET_STATE)
    {
      mb_wm_property_reply (wm,
			    cookies[COOKIE_WIN_NET_STATE],
			    &actual_type_return,
			    &actual_format_return,
			    &nitems_return,
			    &bytes_after_return,
			    &result_atom,
			    &x_error_code);

      cookies[COOKIE_WIN_NET_STATE] = 0;

      if (x_error_code
	  || actual_type_return != XA_ATOM
	  || actual_format_return != 32
	  || nitems_return <= 0
	  || result_atom == NULL
	  )
	{
	  MBWM_DBG("### Warning net type state failed ###");
	  if (x_error_code == BadWindow)
	    goto badwindow_error;
	}
      else
	{
	  int i;
          Atom *a = (Atom*)result_atom;
	  win->ewmh_state = 0;

	  for (i = 0; i < nitems_return; ++i)
	    {
	      if (a[i] == wm->atoms[MBWM_ATOM_NET_WM_STATE_MODAL])
		win->ewmh_state |= MBWMClientWindowEWMHStateModal;
	      else if (a[i] == wm->atoms[MBWM_ATOM_NET_WM_STATE_STICKY])
		win->ewmh_state |= MBWMClientWindowEWMHStateSticky;
	      else if (a[i] == wm->atoms[MBWM_ATOM_NET_WM_STATE_MAXIMIZED_VERT])
		win->ewmh_state |= MBWMClientWindowEWMHStateMaximisedVert;
	      else if (a[i] == wm->atoms[MBWM_ATOM_NET_WM_STATE_MAXIMIZED_HORZ])
		win->ewmh_state |= MBWMClientWindowEWMHStateMaximisedHorz;
	      else if (a[i] == wm->atoms[MBWM_ATOM_NET_WM_STATE_SHADED])
		win->ewmh_state |= MBWMClientWindowEWMHStateShaded;
	      else if (a[i] == wm->atoms[MBWM_ATOM_NET_WM_STATE_SKIP_TASKBAR])
		win->ewmh_state |= MBWMClientWindowEWMHStateSkipTaskbar;
	      else if (a[i] == wm->atoms[MBWM_ATOM_NET_WM_STATE_SKIP_PAGER])
		win->ewmh_state |= MBWMClientWindowEWMHStateSkipPager;
	      else if (a[i] == wm->atoms[MBWM_ATOM_NET_WM_STATE_HIDDEN])
		win->ewmh_state |= MBWMClientWindowEWMHStateHidden;
	      else if (a[i] == wm->atoms[MBWM_ATOM_NET_WM_STATE_FULLSCREEN])
		win->ewmh_state |= MBWMClientWindowEWMHStateFullscreen;
	      else if (a[i] == wm->atoms[MBWM_ATOM_NET_WM_STATE_ABOVE])
		win->ewmh_state |= MBWMClientWindowEWMHStateAbove;
	      else if (a[i] == wm->atoms[MBWM_ATOM_NET_WM_STATE_BELOW])
		win->ewmh_state |= MBWMClientWindowEWMHStateBelow;
	      else if (a[i] ==
		       wm->atoms[MBWM_ATOM_NET_WM_STATE_DEMANDS_ATTENTION])
		win->ewmh_state |= MBWMClientWindowEWMHStateDemandsAttention;
	    }
	}

      changes |= MBWM_WINDOW_PROP_NET_STATE;

      if (result_atom)
	XFree(result_atom);

      result_atom = NULL;
    }

  if (props_req & MBWM_WINDOW_PROP_GEOMETRY)
    {
      if (!mb_wm_xwin_get_geometry_reply (wm,
					  cookies[COOKIE_WIN_GEOM],
					  &win->x_geometry,
					  &foo,
					  &win->depth,
					  &x_error_code))
	{
	  MBWM_DBG("### Warning Get Geometry Failed ( %i ) ###",
		   x_error_code);
	  MBWM_DBG("###   Cookie ID was %li                ###",
		   cookies[COOKIE_WIN_GEOM]);
          cookies[COOKIE_WIN_GEOM] = 0;
	  goto abort;
	}

      cookies[COOKIE_WIN_GEOM] = 0;

      /*
       * FIXME: Monster Hack to make Browser visible (800x480 was a problem)
       *
       * This is a poetic FIXME.  Simply removing this hack has far-reaching
       * consequences, which have even more consequences.  Let's consider the
       * case of Marbles, our most usable application.
       *
       * Marbles is written with SDL and has a game startup window,
       * stacked like this: startup window -> input window -> SDL fullscreen
       * window.  The former two are regular application windows, the latter
       * is an override-redirected one covering the whole screen but is NOT
       * fullscreen (doesn't have the flag).
       *
       * When the input window is mapped this hack kicks in and sets the
       * win->geometry to 799x479, removing it leaves it 800x480.  Later on
       * when the layout manager tries to size and position the new window
       * (mb_wm_layout_real_layout_free()) it checks whether win->geometry
       * is the same as the desktop size (800x480), and if they match it
       * does not request_geometry, effectively not calling MBWMClientTypeApp's
       * method.  This is a problem because then frame_geometry is left 0x0
       * before realizing the client.  And then when MBWMClientBase::realize
       * tries to create the 0x0 frame window it naturally fails.
       *
       * Okay, try removing the hack and make sure mb_wm_layout_real_layout_free()
       * always calls mb_wm_client_app_request_geometry() to set up frame_geometry
       * so client realization actually succeeds.  The next problem is that
       * we have a title bar on the top of the SDL fullscreen widow.
       *
       * Were the hack in effect the SDL fullscreen window would be 799x479,
       * but without it it's 800x480.  This makes a difference for HDRM,
       * which places non-800x480 (clutter geometry!) to app_top, which is
       * above the title bar, so the SDL window would hide it.  But without
       * the hack it's 800x480 and ends up elsewhere, behind the title bar.
       * (Talking about "clutter geometry" is important because ordinary
       *  800x424 windows's clutter geometry is still 800x480, since the
       *  frame geometry is what matters...)
       *
       * We can hack hd_render_manager_set_visibilities() to hide the
       * title bar if it encounters a 800x480 window, regardless its
       * fullscreen flag.  Title bar is gone is Marbles, great. Now,
       * try to close the game.  To cut it short we'll get an assertion
       * failure from tasw because the input window without the hack
       * is hidden (clutter-wise).  We can work it around in a seemingly
       * acceptable way (go to tasw even if the topmost client, which is
       * the input window in this case is not clutter-visible), only to
       * discover that the title bar workaround broke the camera application.
       *
       * Seeing all the complexity we must conclude that whoever conceived
       * the monster hack must have been a genius.
       *
       * ... adding to this, we have a slight problem where the desktop
       * doesn't recieve a button-pressed event (for panning) if we press
       * *right* on the right-hand side of the screen (because it is only 799
       * pixels wide). Hence we'll avoid the monster hack for desktop windows.
       */
      if (win->net_type !=
          wm->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_DESKTOP])
        {
          if (win->x_geometry.width >= wm->xdpy_width)
                  win->x_geometry.width = wm->xdpy_width - 1;
          if (win->x_geometry.height >= wm->xdpy_height)
                  win->x_geometry.height = wm->xdpy_height - 1;
        }

      MBWM_DBG("@@@ New Window Obj @@@");
      MBWM_DBG("Win:  %lx", win->xwindow);
      MBWM_DBG("Type: %lx",win->net_type);
      MBWM_DBG("Geom: +%i+%i,%ix%i",
	       win->x_geometry.x,
	       win->x_geometry.y,
	       win->x_geometry.width,
	       win->x_geometry.height);

      /* FIXME is it right that we directly/immeditaly update
       * win->geometry here, perhaps that should be left as a
       * responsability for a signal handler? */
      win->geometry = win->x_geometry;
    }

  if (props_req & MBWM_WINDOW_PROP_NAME)
    {
      /* We have three options for the name, which in priority order are:
       *  1) _HILDON_WM_NAME, in UTF-8, in XML markup
       *  2) _NET_WM_NAME, in UTF-8, no markup
       *  3) WM_NAME, in ISO 8859-1, no markup
       */
      int name_types[] = {
			  COOKIE_WIN_NAME_UTF8_XML,
			  COOKIE_WIN_NAME_UTF8,
			  COOKIE_WIN_NAME,
			  0
      };
      int *cursor = name_types, name_t = 0;
      char *name = NULL;

      g_free(win->name);
      win->name = NULL;

      while (*cursor)
	{
          char *ret;
          /* Note that we have to get all the replies, otherwise xas.c
           * will leak memory */
	  ret = mb_wm_property_get_reply_and_validate (wm,
                        cookies[*cursor],
		        *cursor == COOKIE_WIN_NAME ?
                                XA_STRING : wm->atoms[MBWM_ATOM_UTF8_STRING],
		        8,
		        0,
		        NULL,
		        &x_error_code);

          cookies[*cursor] = 0;

	  if (x_error_code == BadWindow)
	    goto badwindow_error;

	  if (!name && ret)
            {
	      name = ret;
              name_t = *cursor;
            }
          else if (ret)
            {
              XFree (ret);
              ret = NULL;
            }

	  cursor++;
	}

      switch (name_t)
	{
	case COOKIE_WIN_NAME:
	case COOKIE_WIN_NAME_UTF8:
	  win->name = g_strdup (name);
	  win->name_has_markup = False;
          break;

	case COOKIE_WIN_NAME_UTF8_XML:
	  win->name = g_strdup (name);
	  win->name_has_markup = True;
	  break;

	case 0:
	default:
	  win->name = g_strdup("unknown");
	  win->name_has_markup = False;
	}

      /* FIXME: We could also check the UTF-8 for validity here. */

      /* If they put newlines in, it will look stupid, so
	 take them out. */
      {
	char *newline;

	while ((newline = index (win->name, '\n')))
	  *newline = ' ';
      }

      MBWM_DBG("@@@ New Window Name: '%s' @@@", win->name);

      if (name)
        XFree (name);

      changes |= MBWM_WINDOW_PROP_NAME;
    }

  if (props_req & MBWM_WINDOW_PROP_WM_HINTS)
    {
      XWMHints *wmhints = NULL;

      /* NOTE: pre-R3 X strips group element so will faill for that */
      wmhints = mb_wm_property_get_reply_and_validate (wm,
		                cookies[COOKIE_WIN_WM_HINTS],
			        XA_WM_HINTS,
			        32,
			        NumPropWMHintsElements,
			        NULL,
			        &x_error_code);

      cookies[COOKIE_WIN_WM_HINTS] = 0;

      if (x_error_code == BadWindow)
        goto badwindow_error;

      if (wmhints)
	{
	  MBWM_DBG("@@@ New Window WM Hints @@@");

	  if (wmhints->flags & InputHint)
	    {
	      win->want_key_input = wmhints->input;
	      MBWM_DBG("Want key input: %s", wmhints->input ? "yes" : "no" );
	    }
	  else
	    {
	      MBWM_DBG("No InputHint flag, assuming want key input.");
	      win->want_key_input = True;
	    }

	  if (wmhints->flags & StateHint)
	    {
	      win->initial_state = wmhints->initial_state;
	      MBWM_DBG("Initial State : %i", wmhints->initial_state );
	    }

	  if (wmhints->flags & IconPixmapHint)
	    {
	      win->icon_pixmap = wmhints->icon_pixmap;
	      MBWM_DBG("Icon Pixmap   : %lx", wmhints->icon_pixmap );
	    }

	  if (wmhints->flags & IconMaskHint)
	    {
	      win->icon_pixmap_mask = wmhints->icon_mask;
	      MBWM_DBG("Icon Mask     : %lx", wmhints->icon_mask );
	    }

	  if (wmhints->flags & WindowGroupHint)
	    {
	      win->xwin_group = wmhints->window_group;
	      MBWM_DBG("Window Group  : %lx", wmhints->window_group );
	    }

	  /* NOTE: we ignore icon window stuff */

	  XFree(wmhints);

	  /* FIXME: should track better if thus has changed or not */
	  changes |= MBWM_WINDOW_PROP_WM_HINTS;
	}
      else
        {
          /* Initialize some sane defaults */
          MBWM_DBG("@@@ New Window no WM Hints set, init with defaults @@@");

	  win->want_key_input = True;
	  MBWM_DBG("Want key input: yes");
	}
    }

  typedef struct
  {
    unsigned long       flags;
    unsigned long       functions;
    unsigned long       decorations;
    long                inputMode;
    unsigned long       status;
  } MotifWmHints;

  if (props_req & MBWM_WINDOW_PROP_MWM_HINTS)
    {
      /*
       * Handle the Motif decoration hint (Gtk uses it).
       *
       * We currently only differentiate between decorated and undecorated
       * windows, but do not allow finer customisation of the decor.
       */

      MotifWmHints *mwmhints = NULL;

      mwmhints = mb_wm_property_get_reply_and_validate (wm,
				       cookies[COOKIE_WIN_MWM_HINTS],
				       wm->atoms[MBWM_ATOM_MOTIF_WM_HINTS],
				       32,
				       PROP_MOTIF_WM_HINTS_ELEMENTS,
				       NULL,
				       &x_error_code);

      cookies[COOKIE_WIN_MWM_HINTS] = 0;

      if (x_error_code == BadWindow)
        goto badwindow_error;

      if (mwmhints)
	{
	  MBWM_DBG("@@@ New Window Motif WM Hints @@@");

	  if (mwmhints->flags & MWM_HINTS_DECORATIONS)
	    {
	      if (mwmhints->decorations == 0)
		{
		  win->undecorated = TRUE;
		}
	    }

	  XFree(mwmhints);

	  /* FIXME: should track better if thus has changed or not */
	  changes |= MBWM_WINDOW_PROP_MWM_HINTS;
	}
    }

  if (props_req & MBWM_WINDOW_PROP_PROTOS)
    {
      mb_wm_property_reply (wm,
			    cookies[COOKIE_WIN_PROTOS],
			    &actual_type_return,
			    &actual_format_return,
			    &nitems_return,
			    &bytes_after_return,
			    &result_atom,
			    &x_error_code);

      cookies[COOKIE_WIN_PROTOS] = 0;

      if (x_error_code
	  || actual_type_return != XA_ATOM
	  || actual_format_return != 32
	  || result_atom == NULL
	  )
	{
	  MBWM_DBG("### Warning wm protocols prop failed ###");
          if (x_error_code == BadWindow)
            goto badwindow_error;
	}
      else
	{
	  int i;
          Atom *a = (Atom*)result_atom;

	  for (i = 0; i < nitems_return; ++i)
	    {
	      if (a[i] == wm->atoms[MBWM_ATOM_WM_TAKE_FOCUS])
		win->protos |= MBWMClientWindowProtosFocus;
	      else if (a[i] == wm->atoms[MBWM_ATOM_WM_DELETE_WINDOW])
		win->protos |= MBWMClientWindowProtosDelete;
	      else if (a[i] == wm->atoms[MBWM_ATOM_NET_WM_CONTEXT_HELP])
		win->protos |= MBWMClientWindowProtosContextHelp;
	      else if (a[i] == wm->atoms[MBWM_ATOM_NET_WM_CONTEXT_ACCEPT])
		win->protos |= MBWMClientWindowProtosContextAccept;
	      else if (a[i] == wm->atoms[MBWM_ATOM_NET_WM_CONTEXT_CUSTOM])
		win->protos |= MBWMClientWindowProtosContextCustom;
	      else if (a[i] == wm->atoms[MBWM_ATOM_NET_WM_PING])
		win->protos |= MBWMClientWindowProtosPing;
	      else if (a[i] == wm->atoms[MBWM_ATOM_NET_WM_SYNC_REQUEST])
		win->protos |= MBWMClientWindowProtosSyncRequest;
	      else
		MBWM_DBG("Unhandled WM Protocol %lu", ((Atom*)result_atom)[i]);
	    }
	}

      if (result_atom)
	XFree(result_atom);

      changes |= MBWM_WINDOW_PROP_PROTOS;

      result_atom = NULL;
    }

  if (props_req & MBWM_WINDOW_PROP_CLIENT_MACHINE)
    {
      if (win->machine)
	XFree(win->machine);

      win->machine
	= mb_wm_property_get_reply_and_validate (wm,
						 cookies[COOKIE_WIN_MACHINE],
						 XA_STRING,
						 8,
						 0,
						 NULL,
						 &x_error_code);

      cookies[COOKIE_WIN_MACHINE] = 0;

      if (x_error_code == BadWindow)
        goto badwindow_error;

      if (!win->machine)
	{
	  MBWM_DBG("### Warning wm client machine prop failed ###");
	}
      else
	{
	  MBWM_DBG("@@@ Client machine %s @@@", win->machine);
	}
    }

  if (props_req & MBWM_WINDOW_PROP_LIVE_BACKGROUND)
    {
      int *ret;
      ret = mb_wm_property_get_reply_and_validate (wm,
				cookies[COOKIE_WIN_LIVE_BACKGROUND],
						 XA_INTEGER,
						 32,
						 1,
						 NULL,
						 &x_error_code);

      cookies[COOKIE_WIN_LIVE_BACKGROUND] = 0;
      if (ret)
        win->live_background = *ret;
      else
        win->live_background = 0;

      if (x_error_code == BadWindow)
        goto badwindow_error;

      if (ret)
        XFree (ret);
    }

  if (props_req & MBWM_WINDOW_PROP_NET_PID)
    {
      unsigned char *pid = NULL;

      mb_wm_property_reply (wm,
			    cookies[COOKIE_WIN_PID],
			    &actual_type_return,
			    &actual_format_return,
			    &nitems_return,
			    &bytes_after_return,
			    &pid,
			    &x_error_code);

      cookies[COOKIE_WIN_PID] = 0;

      if (x_error_code
	  || actual_type_return != XA_CARDINAL
	  || actual_format_return != 32
	  || pid == NULL
	  )
	{
	  MBWM_DBG("### Warning net pid prop failed ###");
          if (x_error_code == BadWindow)
            goto badwindow_error;
	}
      else
	{
	  win->pid = *(pid_t *)pid;
	  MBWM_DBG("@@@ pid %d @@@", win->pid);
	}

      if (pid)
	XFree(pid);
    }

  if (props_req & MBWM_WINDOW_PROP_CM_TRANSLUCENCY)
    {
      unsigned char *translucency = NULL;

      mb_wm_property_reply (wm,
			    cookies[COOKIE_WIN_CM_TRANSLUCENCY],
			    &actual_type_return,
			    &actual_format_return,
			    &nitems_return,
			    &bytes_after_return,
			    &translucency,
			    &x_error_code);

      cookies[COOKIE_WIN_CM_TRANSLUCENCY] = 0;

      if (x_error_code
	  || actual_type_return != XA_CARDINAL
	  || actual_format_return != 32
	  || translucency == NULL
	  )
	{
	  MBWM_DBG("### no CM_TRANSLUCENCY prop ###");
	  win->translucency = -1;
          if (x_error_code == BadWindow)
            goto badwindow_error;
	}
      else
	{
	  win->translucency = (int)*translucency;
	  MBWM_DBG("@@@ translucency %d @@@", win->translucency);
	}

      changes |= MBWM_WINDOW_PROP_CM_TRANSLUCENCY;

      if (translucency)
	XFree (translucency);
    }

  if (props_req & MBWM_WINDOW_PROP_NO_TRANSITIONS)
    {
      mb_wm_property_reply (wm,
			    cookies[COOKIE_WIN_NO_TRANSITIONS],
			    &actual_type_return,
			    &actual_format_return,
			    &nitems_return,
			    &bytes_after_return,
			    &result_atom,
			    &x_error_code);

      cookies[COOKIE_WIN_NO_TRANSITIONS] = 0;

      if (x_error_code
	  || actual_type_return != XA_CARDINAL
	  || actual_format_return != 32
	  || result_atom == NULL
	  )
	{
	  MBWM_DBG("### Warning no transitions prop failed ###");
          if (x_error_code == BadWindow)
            goto badwindow_error;
	}
      else
	{
	  win->allowed_actions |= MBWMClientWindowActionNoTransitions;
	}

      if (result_atom)
	XFree(result_atom);

      changes |= MBWM_WINDOW_PROP_NO_TRANSITIONS;

      result_atom = NULL;
    }

  if (props_req & MBWM_WINDOW_PROP_NET_USER_TIME)
    {
      unsigned char *user_time = NULL;

      mb_wm_property_reply (wm,
			    cookies[COOKIE_WIN_USER_TIME],
			    &actual_type_return,
			    &actual_format_return,
			    &nitems_return,
			    &bytes_after_return,
			    &user_time,
			    &x_error_code);

      cookies[COOKIE_WIN_USER_TIME] = 0;

      if (x_error_code
	  || actual_type_return != XA_CARDINAL
	  || actual_format_return != 32
	  || user_time == NULL
	  )
	{
	  MBWM_DBG("### Warning _NET_WM_USER_TIME failed ###");
          if (x_error_code == BadWindow)
	    {
              if (user_time)
	        XFree(user_time);
              goto badwindow_error;
	    }
	}
      else
	{
	  win->user_time = (unsigned long)*user_time;
	  MBWM_DBG("@@@ user time %lu @@@", win->user_time);
	}

      if (user_time)
	XFree(user_time);
    }

  if (props_req & MBWM_WINDOW_PROP_HILDON_STACKING)
    {
      unsigned char *value = NULL;

      mb_wm_property_reply (wm,
			    cookies[COOKIE_WIN_HILDON_STACKING],
			    &actual_type_return,
			    &actual_format_return,
			    &nitems_return,
			    &bytes_after_return,
			    &value,
			    &x_error_code);

      cookies[COOKIE_WIN_HILDON_STACKING] = 0;

      if (x_error_code
	  || actual_type_return != XA_CARDINAL
	  || actual_format_return != 32
	  || value == NULL
	  )
	{
	  win->hildon_stacking_layer = 0;
          if (x_error_code == BadWindow)
	    {
              if (value)
	        XFree(value);
              goto badwindow_error;
	    }
	}
      else
	{
	  win->hildon_stacking_layer = (unsigned int)*value;
	}

      if (value)
	XFree(value);

      changes |= MBWM_WINDOW_PROP_HILDON_STACKING;
    }

  if (props_req & MBWM_WINDOW_PROP_PORTRAIT)
    {
      unsigned char *value;

      value = NULL;
      mb_wm_property_reply (wm, cookies[COOKIE_WIN_PORTRAIT_SUPPORT],
			    &actual_type_return, &actual_format_return,
			    &nitems_return, &bytes_after_return,
			    &value, &x_error_code);
      cookies[COOKIE_WIN_PORTRAIT_SUPPORT] = 0;
      win->portrait_supported = !x_error_code && value
          && actual_type_return == XA_CARDINAL && actual_format_return == 32
        ? *(unsigned *)value : -1;
      if (value)
        XFree(value);
      if (x_error_code == BadWindow)
          goto badwindow_error;

      value = NULL;
      mb_wm_property_reply (wm, cookies[COOKIE_WIN_PORTRAIT_REQUEST],
			    &actual_type_return, &actual_format_return,
			    &nitems_return, &bytes_after_return,
			    &value, &x_error_code);
      cookies[COOKIE_WIN_PORTRAIT_REQUEST] = 0;
      win->portrait_requested = !x_error_code && value
        && actual_type_return == XA_CARDINAL && actual_format_return == 32
        ? *(unsigned *)value : -1;
      if (value)
        XFree(value);
      if (x_error_code == BadWindow)
          goto badwindow_error;

      changes |= MBWM_WINDOW_PROP_PORTRAIT;
    }

  if (changes)
    mb_wm_object_signal_emit (MB_WM_OBJECT (win), changes);

  return True;

abort:

  abort_exit = True;

badwindow_error:

  /************************************************/
  /* handle skipped replies to avoid memory leaks */
  /************************************************/

  if (cookies[COOKIE_WIN_ATTR])
    {
      xwin_attr = mb_wm_xwin_get_attributes_reply (wm,
						   cookies[COOKIE_WIN_ATTR],
						   &x_error_code);
      if (xwin_attr)
        XFree (xwin_attr);
    }

  if (cookies[COOKIE_WIN_NAME])
    {
      int name_types[] = {
			  COOKIE_WIN_NAME_UTF8_XML,
			  COOKIE_WIN_NAME_UTF8,
			  COOKIE_WIN_NAME,
			  0
      };
      int *cursor;

      for (cursor = name_types; *cursor; ++cursor)
	{
          if (cookies[*cursor])
            {
              char *ret;
              ret = mb_wm_property_get_reply_and_validate (wm,
                        cookies[*cursor],
		        *cursor == COOKIE_WIN_NAME ?
                                XA_STRING : wm->atoms[MBWM_ATOM_UTF8_STRING],
		        8,
		        0,
		        NULL,
		        &x_error_code);
              if (ret)
                XFree (ret);
            }
        }
    }

  if (cookies[COOKIE_WIN_WM_HINTS])
    {
      XWMHints *wmhints;
      wmhints = mb_wm_property_get_reply_and_validate (wm,
		                cookies[COOKIE_WIN_WM_HINTS],
			        XA_WM_HINTS,
			        32,
			        NumPropWMHintsElements,
			        NULL,
			        &x_error_code);
      if (wmhints)
        XFree (wmhints);
    }

  if (cookies[COOKIE_WIN_LIVE_BACKGROUND])
    {
      int *ret;
      ret = mb_wm_property_get_reply_and_validate (wm,
		                cookies[COOKIE_WIN_LIVE_BACKGROUND],
			        XA_INTEGER,
			        32,
			        1,
			        NULL,
			        &x_error_code);
      if (ret)
        XFree (ret);
    }

  if (cookies[COOKIE_WIN_MWM_HINTS])
    {
      MotifWmHints *mwmhints;
      mwmhints = mb_wm_property_get_reply_and_validate (wm,
				       cookies[COOKIE_WIN_MWM_HINTS],
				       wm->atoms[MBWM_ATOM_MOTIF_WM_HINTS],
				       32,
				       PROP_MOTIF_WM_HINTS_ELEMENTS,
				       NULL,
				       &x_error_code);
      if (mwmhints)
        XFree (mwmhints);
    }

  if (cookies[COOKIE_WIN_TRANSIENCY])
    {
      Window *trans_win;
      trans_win = mb_wm_property_get_reply_and_validate (wm,
					 cookies[COOKIE_WIN_TRANSIENCY],
					 MBWM_ATOM_WM_TRANSIENT_FOR,
					 32,
					 1,
					 NULL,
					 &x_error_code);
      if (trans_win)
        XFree (trans_win);
    }

  if (cookies[COOKIE_WIN_MACHINE])
    {
      char *m;
      m = mb_wm_property_get_reply_and_validate (wm,
						 cookies[COOKIE_WIN_MACHINE],
						 XA_STRING,
						 8,
						 0,
						 NULL,
						 &x_error_code);
      if (m)
        XFree (m);
    }

  {
    int name_types[] = {
		        COOKIE_WIN_PROTOS,
                        COOKIE_WIN_PID,
                        COOKIE_WIN_USER_TIME,
                        COOKIE_WIN_CM_TRANSLUCENCY,
                        COOKIE_WIN_HILDON_STACKING,
                        COOKIE_WIN_PORTRAIT_SUPPORT,
                        COOKIE_WIN_PORTRAIT_REQUEST,
                        COOKIE_WIN_NET_STATE,
                        COOKIE_WIN_TYPE,
                        COOKIE_WIN_HILDON_TYPE,
                        COOKIE_WIN_NO_TRANSITIONS,
                        0
    };
    int *cursor;
    for (cursor = name_types; *cursor; ++cursor)
      {
        if (cookies[*cursor])
          {
            result_atom = NULL;
            mb_wm_property_reply (wm,
			    cookies[*cursor],
			    &actual_type_return,
			    &actual_format_return,
			    &nitems_return,
			    &bytes_after_return,
			    &result_atom,
			    &x_error_code);
            if (result_atom)
              XFree (result_atom);
          }
      }
  }

  if (cookies[COOKIE_WIN_GEOM])
    {
      MBGeometry geo;
      unsigned border, depth;

      mb_wm_xwin_get_geometry_reply (wm,
                                     cookies[COOKIE_WIN_GEOM],
                                     &geo, &border, &depth,
                                     &x_error_code);
    }

  if (abort_exit)
    return True;
  else
    return False;
}

Bool
mb_wm_client_window_is_state_set (MBWMClientWindow *win,
				  MBWMClientWindowEWMHState state)
{
  return (win->ewmh_state & state) ? True : False;
}

