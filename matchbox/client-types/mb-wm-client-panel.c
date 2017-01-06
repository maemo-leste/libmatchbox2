#include "mb-wm-client-panel.h"
#include "mb-wm-theme.h"

static void
mb_wm_client_panel_realize (MBWindowManagerClient *client);

static Bool
mb_wm_client_panel_request_geometry (MBWindowManagerClient *client,
				     MBGeometry            *new_geometry,
				     MBWMClientReqGeomType  flags);

static MBWMStackLayerType
mb_wm_client_panel_stacking_layer (MBWindowManagerClient *client);

static void
mb_wm_client_panel_stack (MBWindowManagerClient *client, intptr_t flags);

static void
mb_wm_client_panel_class_init (MBWMObjectClass *klass)
{
  MBWindowManagerClientClass *client;

  client = (MBWindowManagerClientClass *)klass;

  client->client_type = MBWMClientTypePanel;

  client->realize        = mb_wm_client_panel_realize;
  client->geometry       = mb_wm_client_panel_request_geometry;
  client->stacking_layer = mb_wm_client_panel_stacking_layer;
  client->stack          = mb_wm_client_panel_stack;

#if MBWM_WANT_DEBUG
  klass->klass_name = "MBWMClientPanel";
#endif
}

static int
mb_wm_client_panel_init (MBWMObject *this, va_list vap)
{
  MBWindowManagerClient * client  = MB_WM_CLIENT (this);
  MBGeometry            * win_geo = &client->window->geometry;
  MBGeometry              geom;
  MBWMTheme             * theme = client->wmref->theme;

  if (theme && mb_wm_theme_get_client_geometry (theme, client, &geom))
    {
      /* The theme prescribes geometry for the panel, resize the
       * window accordingly
       */
      MBWMClientWindow * win = client->window;
      MBWindowManager  * wm = client->wmref;
      int g_x, g_y, x, y, w, h;
      unsigned int g_w, g_h, g_bw, g_d;

      x = geom.x;
      y = geom.y;
      w = geom.width;
      h = geom.height;

      /* If some of the theme values are unset, we have to get the current
       * values for the window in their place -- this is a round trip, so
       * we only do this when necessary
       */
      if (x < 0 || y < 0 || w < 0 || h < 0)
	{
	  Window root;
	  XGetGeometry (wm->xdpy, win->xwindow,
			&root, &g_x, &g_y, &g_w, &g_h, &g_bw, &g_d);
	}

      if (x < 0)
	x = g_x;

      if (y < 0)
	y = g_y;

      if (w < 0)
	w = g_w;

      if (h < 0)
	h = g_h;

      win->geometry.x = x;
      win->geometry.y = y;
      win->geometry.width  = w;
      win->geometry.height = h;
      
      mb_wm_client_geometry_mark_dirty (client);
    }

  if (!client->layout_hints)
    {
      /*
       * The theme did not prescribe layout hints, work something out
       */
      MBWMClientLayoutHints hints = LayoutPrefVisible;

      if (win_geo->width > win_geo->height)
	if (win_geo->y < (client->wmref->xdpy_height/2))
	  hints |= LayoutPrefReserveEdgeNorth;
	else
	  hints |= LayoutPrefReserveEdgeSouth;
      else
	if (win_geo->x < (client->wmref->xdpy_width/2))
	  hints |= LayoutPrefReserveEdgeWest;
	else
	  hints |= LayoutPrefReserveEdgeEast;

      mb_wm_client_set_layout_hints (client, hints);
    }
  else
    {
      /* Make sure we are marked as visible */
      mb_wm_client_set_layout_hint (client, LayoutPrefVisible, True);
    }

  if (client->layout_hints & LayoutPrefOverlaps)
    client->stacking_layer = MBWMStackLayerTopMid;
  else
    client->stacking_layer = MBWMStackLayerBottomMid;

  return 1;
}

static void
mb_wm_client_panel_destroy (MBWMObject *this)
{
}

int
mb_wm_client_panel_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (MBWMClientPanelClass),
	sizeof (MBWMClientPanel),
	mb_wm_client_panel_init,
	mb_wm_client_panel_destroy,
	mb_wm_client_panel_class_init
      };

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_CLIENT_BASE, 0);
    }

  return type;
}

static void
mb_wm_client_panel_realize (MBWindowManagerClient *client)
{
  /*
   * Must reparent the window to our root, otherwise we restacking of
   * pre-existing windows might fail.
   */
  XReparentWindow(client->wmref->xdpy, MB_WM_CLIENT_XWIN(client),
		  client->wmref->root_win->xwindow, 0, 0);

  return;
}

static Bool
mb_wm_client_panel_request_geometry (MBWindowManagerClient *client,
				     MBGeometry            *new_geometry,
				     MBWMClientReqGeomType  flags)
{
  if (client->window->geometry.x != new_geometry->x
      || client->window->geometry.y != new_geometry->y
      || client->window->geometry.width  != new_geometry->width
      || client->window->geometry.height != new_geometry->height)
    {
      client->window->geometry.x = new_geometry->x;
      client->window->geometry.y = new_geometry->y;
      client->window->geometry.width  = new_geometry->width;
      client->window->geometry.height = new_geometry->height;

      mb_wm_client_geometry_mark_dirty (client);
    }

  return True;
}

static void
mb_wm_client_panel_stack (MBWindowManagerClient *client, intptr_t flags)
{
  /*
   * If this is 'normal' panel, we stack with the default value.
   *
   * If this is an overlaping panel, stack immediately above the top-level
   * application.
   */
  if (!(client->layout_hints & LayoutPrefOverlaps))
    {
      mb_wm_stack_move_top (client);
      return;
    }

  mb_wm_stack_move_client_above_type (client,
				     MBWMClientTypeApp|MBWMClientTypeDesktop);
}

static MBWMStackLayerType
mb_wm_client_panel_stacking_layer (MBWindowManagerClient *client)
{
  /*
   * If we are showing desktop, ensure that we stack above it.
   */
  if (client->wmref->flags & MBWindowManagerFlagDesktop)
    return MBWMStackLayerTopMid;

  return client->stacking_layer;
}

MBWindowManagerClient*
mb_wm_client_panel_new(MBWindowManager *wm, MBWMClientWindow *win)
{
  MBWindowManagerClient *client = NULL;

  client = MB_WM_CLIENT(mb_wm_object_new (MB_WM_TYPE_CLIENT_PANEL,
					  MBWMObjectPropWm,           wm,
					  MBWMObjectPropClientWindow, win,
					  NULL));

  return client;
}


