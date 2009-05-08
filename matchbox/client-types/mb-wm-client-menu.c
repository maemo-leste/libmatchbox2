#include "mb-wm-client-menu.h"

#include "mb-wm-theme.h"

static Bool
mb_wm_client_menu_request_geometry (MBWindowManagerClient *client,
				    MBGeometry            *new_geometry,
				    MBWMClientReqGeomType  flags);

static void
mb_wm_client_menu_realize (MBWindowManagerClient *client)
{
  /*
   * Must reparent the window to our root, otherwise we restacking of
   * pre-existing windows might fail.
   */
  XReparentWindow(client->wmref->xdpy, MB_WM_CLIENT_XWIN(client),
		  client->wmref->root_win->xwindow, 0, 0);
}

static void
mb_wm_client_menu_class_init (MBWMObjectClass *klass)
{
  MBWindowManagerClientClass *client;

  MBWM_MARK();

  client = (MBWindowManagerClientClass *)klass;

  client->client_type  = MBWMClientTypeMenu;
  client->geometry     = mb_wm_client_menu_request_geometry;
  client->realize      = mb_wm_client_menu_realize;

#if MBWM_WANT_DEBUG
  klass->klass_name = "MBWMClientMenu";
#endif
}

static void
mb_wm_client_menu_destroy (MBWMObject *this)
{
}

static int
mb_wm_client_menu_init (MBWMObject *this, va_list vap)
{
  MBWindowManagerClient *client      = MB_WM_CLIENT (this);
  MBWindowManager       *wm          = client->wmref;
  MBWMClientWindow      *win         = client->window;
  MBGeometry geom;
  Atom actions[] = {
    wm->atoms[MBWM_ATOM_NET_WM_ACTION_CLOSE],
    wm->atoms[MBWM_ATOM_NET_WM_ACTION_MOVE],
    wm->atoms[MBWM_ATOM_NET_WM_ACTION_RESIZE],
  };

  XChangeProperty (wm->xdpy, win->xwindow,
		   wm->atoms[MBWM_ATOM_NET_WM_ALLOWED_ACTIONS],
		   XA_ATOM, 32, PropModeReplace,
		   (unsigned char *)actions,
		   sizeof (actions)/sizeof (actions[0]));

  mb_wm_client_set_layout_hints (client,
				 LayoutPrefPositionFree|LayoutPrefVisible|
				 LayoutPrefFixedX|LayoutPrefFixedY);

  /*
   * Stack menus on the top of the stacking order, regardless of whether they
   * declare themselves transient or not.
   *
   * (Gtk menus do kbd and pointer grabs and do not take kindly to being
   * restacked.)
   */
#if 0
  if (win->xwin_transient_for
      && win->xwin_transient_for != win->xwindow
      && win->xwin_transient_for != wm->root_win->xwindow)
    {
      MBWM_DBG ("Adding to '%lx' transient list",
		win->xwin_transient_for);
      mb_wm_client_add_transient
	(mb_wm_managed_client_from_xwindow (wm,
					    win->xwin_transient_for),
	 client);
      client->stacking_layer = 0;  /* We stack with whatever transient too */
    }
  else
    {
      MBWM_DBG ("Menu is transient to root");
      /* Stack with 'always on top' */
      client->stacking_layer = MBWMStackLayerTopMid;
    }
#endif

  if (win->hildon_stacking_layer == 0)
    client->stacking_layer = MBWMStackLayerTop;
  else
    client->stacking_layer = win->hildon_stacking_layer
                             + MBWMStackLayerHildon1 - 1;

  geom = client->window->geometry;

  if (((MBWindowManagerClientClass*)mb_wm_object_get_class(this))->client_type
      != MBWMClientTypeMenu)
  {
    /* new-style menu */

    geom.y = 0;

    if (wm->xdpy_width > wm->xdpy_height)
      {
	/* Landscape mode: menu has a gutter either side */
	geom.width = wm->xdpy_width - 100;
	geom.x = wm->xdpy_width / 2 - geom.width / 2;
      }
    else
      {
	/* Portrait mode: menu takes up the full width */
	geom.width = wm->xdpy_width;
	geom.x = 0;
      }
  }
  else
  {
    gint title_x, title_y;

    mb_wm_theme_get_title_xy (wm->theme, &title_x, &title_y);

    if (geom.x < title_x)
      geom.x = title_x;

    if (geom.y < title_y)
      geom.y = title_y;
  }

  g_debug ("%s: Menu will be at %d %d %d %d", __func__, geom.x, geom.y,
           geom.width, geom.height);

  mb_wm_client_menu_request_geometry (client, &geom,
				      MBWMClientReqGeomForced);

  return 1;
}

int
mb_wm_client_menu_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (MBWMClientMenuClass),
	sizeof (MBWMClientMenu),
	mb_wm_client_menu_init,
	mb_wm_client_menu_destroy,
	mb_wm_client_menu_class_init
      };

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_CLIENT_BASE, 0);
    }

  return type;
}

static Bool
mb_wm_client_menu_request_geometry (MBWindowManagerClient *client,
				    MBGeometry            *new_geometry,
				    MBWMClientReqGeomType  flags)
{
  if (client->window->geometry.x != new_geometry->x
      || client->window->geometry.y != new_geometry->y
      || client->window->geometry.width  != new_geometry->width
      || client->window->geometry.height != new_geometry->height)
    {
      client->window->geometry.x      = new_geometry->x;
      client->window->geometry.y      = new_geometry->y;
      client->window->geometry.width  = new_geometry->width;
      client->window->geometry.height = new_geometry->height;

      mb_wm_client_geometry_mark_dirty (client);

      return True; /* Geometry accepted */
    }

  return True; /* Geometry accepted */
}

MBWindowManagerClient*
mb_wm_client_menu_new (MBWindowManager *wm, MBWMClientWindow *win)
{
  MBWindowManagerClient *client;

  client
    = MB_WM_CLIENT(mb_wm_object_new (MB_WM_TYPE_CLIENT_MENU,
				     MBWMObjectPropWm,           wm,
				     MBWMObjectPropClientWindow, win,
				     NULL));

  return client;
}

