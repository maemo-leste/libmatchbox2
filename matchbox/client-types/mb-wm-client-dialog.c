#include "mb-wm-client-dialog.h"

#include "mb-wm-theme.h"

static Bool
mb_wm_client_dialog_request_geometry (MBWindowManagerClient *client,
				      MBGeometry            *new_geometry,
				      MBWMClientReqGeomType  flags);

static void
mb_wm_client_dialog_theme_change (MBWindowManagerClient *client);

static void
mb_wm_client_dialog_show (MBWindowManagerClient *client)
{
  MBWindowManagerClientClass  *parent_klass = NULL;

  /*
   * We need the parent of the MBWMClientDialogClass to chain up
   */
  if (MB_WM_IS_CLIENT_DIALOG (client))
    parent_klass = MB_WM_CLIENT_CLASS (MB_WM_OBJECT_GET_PARENT_CLASS (client));
  else
    {
      /*
       * A derived klass -- need to traverse the klass hierarchy to get to
       * the dialog klass
       */
      MBWMObjectClass * object_klass = MB_WM_OBJECT_GET_PARENT_CLASS (client);

      while (object_klass && object_klass->type != MB_WM_TYPE_CLIENT_DIALOG)
	object_klass = object_klass->parent;

      if (object_klass && object_klass->parent)
	parent_klass = MB_WM_CLIENT_CLASS (object_klass->parent);
    }

  if (client->transient_for != NULL)
    {
      MBWindowManager * wm = client->wmref;

      /*
       * If an attempt has been made to activate a hidden
       * dialog, activate its parent app first.
       *
       * Note this is mainly to work with some task selectors
       * ( eg the gnome one, which activates top dialog ).
       */
      MBWindowManagerClient *parent = client->transient_for;

      while (parent->transient_for != NULL)
	parent = parent->transient_for;

      if (parent != mb_wm_get_visible_main_client(wm))
	mb_wm_client_show (parent);
    }

  if (parent_klass && parent_klass->show)
    parent_klass->show(client);
}

static void
mb_wm_client_dialog_class_init (MBWMObjectClass *klass)
{
  MBWindowManagerClientClass *client;

  MBWM_MARK();

  client = (MBWindowManagerClientClass *)klass;

  client->client_type  = MBWMClientTypeDialog;
  client->geometry     = mb_wm_client_dialog_request_geometry;
  client->show         = mb_wm_client_dialog_show;
  client->theme_change = mb_wm_client_dialog_theme_change;

#if MBWM_WANT_DEBUG
  klass->klass_name = "MBWMClientDialog";
#endif
}

static void
mb_wm_client_dialog_destroy (MBWMObject *this)
{
}

static int
mb_wm_client_dialog_init (MBWMObject *this, va_list vap)
{
  MBWindowManagerClient *client = MB_WM_CLIENT (this);
  MBWindowManager       *wm = client->wmref;
  MBWMClientWindow      *win = client->window;
  MBGeometry             geom;
  Atom actions[] = {
    wm->atoms[MBWM_ATOM_NET_WM_ACTION_CLOSE],
    wm->atoms[MBWM_ATOM_NET_WM_ACTION_MOVE],
  };

  XChangeProperty (wm->xdpy, win->xwindow,
		   wm->atoms[MBWM_ATOM_NET_WM_ALLOWED_ACTIONS],
		   XA_ATOM, 32, PropModeReplace,
		   (unsigned char *)actions,
		   sizeof (actions)/sizeof (actions[0]));


  mb_wm_client_set_layout_hints (client,
				 LayoutPrefPositionFree |
				 LayoutPrefMovable      |
				 LayoutPrefVisible);

  if (!client->window->undecorated && wm->theme)
    {
      mb_wm_theme_create_decor (wm->theme, client, MBWMDecorTypeNorth);
      mb_wm_theme_create_decor (wm->theme, client, MBWMDecorTypeSouth);
      mb_wm_theme_create_decor (wm->theme, client, MBWMDecorTypeWest);
      mb_wm_theme_create_decor (wm->theme, client, MBWMDecorTypeEast);
    }

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
      if (win->hildon_stacking_layer == 0)
        /* Stack with 'always on top' */
        client->stacking_layer = MBWMStackLayerTopMid;
      else
        client->stacking_layer = client->window->hildon_stacking_layer
                                 + MBWMStackLayerHildon1 - 1;
    }

  /* center if window sets 0,0
   * Only do this for dialogs, not derived classes.
   * FIXME needs to work on frame, not window.
   */
  if (MB_WM_IS_CLIENT_DIALOG (client) &&
      client->window->geometry.x == 0 && client->window->geometry.y == 0)
    {
        MBGeometry  avail_geom;

	mb_wm_get_display_geometry (wm, &avail_geom);

	client->window->geometry.x
	  = (avail_geom.width - client->window->geometry.width) / 2;
	client->window->geometry.y
	  = (avail_geom.height - client->window->geometry.height) / 2;
    }

  mb_wm_client_geometry_mark_dirty (client);
  mb_wm_client_visibility_mark_dirty (client);

  if (!wm->theme)
    return 1;

  /*
   * Since dialogs are free-sized, they do not necessarily get a request for
   * geometry from the layout manager -- we have to set the initial geometry
   * here
   */
  if (client->window->undecorated)
    {
      geom.x      = client->window->geometry.x;
      geom.y      = client->window->geometry.y;
      geom.width  = client->window->geometry.width;
      geom.height = client->window->geometry.height;
    }
  else
    {
      int n, s, w, e;
      n = s = w = e = 0;

      mb_wm_theme_get_decor_dimensions (wm->theme, client, &n, &s, &w, &e);

      geom.x      = client->window->geometry.x;
      geom.y      = client->window->geometry.y;
      geom.width  = client->window->geometry.width + w + e;
      geom.height = client->window->geometry.height + n + s;
    }

  mb_wm_client_dialog_request_geometry (client, &geom,
					MBWMClientReqGeomForced);

  return 1;
}

int
mb_wm_client_dialog_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (MBWMClientDialogClass),
	sizeof (MBWMClientDialog),
	mb_wm_client_dialog_init,
	mb_wm_client_dialog_destroy,
	mb_wm_client_dialog_class_init
      };

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_CLIENT_BASE, 0);
    }

  return type;
}

static Bool
mb_wm_client_dialog_request_geometry (MBWindowManagerClient *client,
				      MBGeometry            *new_geometry,
				      MBWMClientReqGeomType  flags)
{
  const MBGeometry * geom;
  Bool               change_pos;
  Bool               change_size;

  /*
   * When we get an internal geometry request, like from the layout manager,
   * the new geometry applies to the frame; however, if the request is
   * external from ConfigureRequest, it is new geometry of the client window,
   * so we need to take care to handle this right.
   */
  geom = (flags & MBWMClientReqGeomIsViaConfigureReq) ?
    &client->window->geometry : &client->frame_geometry;

  change_pos = (geom->x != new_geometry->x || geom->y != new_geometry->y);

  change_size = (geom->width  != new_geometry->width ||
		 geom->height != new_geometry->height);

  if (change_size)
    {
      int north = 0, south = 0, west = 0, east = 0;
      MBWindowManager *wm = client->wmref;

      if (!client->window->undecorated && client->decor)
	mb_wm_theme_get_decor_dimensions (wm->theme, client,
					  &north, &south, &west, &east);

      if (flags & MBWMClientReqGeomIsViaConfigureReq)
	{
	  /*
	   * Calculate the frame size from the window size
	   */
	  MBWM_DBG ("ConfigureRequest [%d,%d;%dx%d] -> [%d,%d;%dx%d]\n",
		    client->window->geometry.x,
		    client->window->geometry.y,
		    client->window->geometry.width,
		    client->window->geometry.height,
		    new_geometry->x,
		    new_geometry->y,
		    new_geometry->width,
		    new_geometry->height);

	  client->window->geometry.x      = new_geometry->x;
	  client->window->geometry.y      = new_geometry->y;
	  client->window->geometry.width  = new_geometry->width;
	  client->window->geometry.height = new_geometry->height;

	  client->frame_geometry.x
	    = client->window->geometry.x - west;
	  client->frame_geometry.y
	    = client->window->geometry.y - north;
	  client->frame_geometry.width
	    = client->window->geometry.width + (west + east);
	  client->frame_geometry.height
	    = client->window->geometry.height + (south + north);
	}
      else
	{
	  /*
	   * Internal request, e.g., from layout manager; work out client
	   * window size from the provided frame size.
	   */
	  client->frame_geometry.x      = new_geometry->x;
	  client->frame_geometry.y      = new_geometry->y;
	  client->frame_geometry.width  = new_geometry->width;
	  client->frame_geometry.height = new_geometry->height;

	  client->window->geometry.x
	    = client->frame_geometry.x + west;
	  client->window->geometry.y
	    = client->frame_geometry.y + north;
	  client->window->geometry.width
	    = client->frame_geometry.width - (west + east);
	  client->window->geometry.height
	    = client->frame_geometry.height - (south + north);
	}

      mb_wm_client_geometry_mark_dirty (client);

      return True; /* Geometry accepted */
    }
  else if (change_pos)
    {
      /*
       * Change of position only, just move both windows, no need to
       * mess about with the decor.
       */
      int x_diff = geom->x - new_geometry->x;
      int y_diff = geom->y - new_geometry->y;

      client->frame_geometry.x -= x_diff;
      client->frame_geometry.y -= y_diff;
      client->window->geometry.x -= x_diff;
      client->window->geometry.y -= y_diff;

      mb_wm_client_geometry_mark_dirty (client);

      return True;
    }

  return True; /* Geometry accepted */
}

static void
mb_wm_client_dialog_theme_change (MBWindowManagerClient *client)
{
  MBWMList * l = client->decor;

  while (l)
    {
      MBWMDecor * d = l->data;
      MBWMList * n = l->next;

      mb_wm_object_unref (MB_WM_OBJECT (d));
      free (l);

      l = n;
    }

  client->decor = NULL;

  if (!client->window->undecorated)
    {
      mb_wm_theme_create_decor (client->wmref->theme,
				client, MBWMDecorTypeNorth);

      mb_wm_theme_create_decor (client->wmref->theme,
				client, MBWMDecorTypeSouth);

      mb_wm_theme_create_decor (client->wmref->theme,
				client, MBWMDecorTypeWest);

      mb_wm_theme_create_decor (client->wmref->theme,
				client, MBWMDecorTypeEast);
    }

  mb_wm_client_geometry_mark_dirty (client);
  mb_wm_client_visibility_mark_dirty (client);
}


MBWindowManagerClient*
mb_wm_client_dialog_new (MBWindowManager *wm, MBWMClientWindow *win)
{
  MBWindowManagerClient *client;

  client
    = MB_WM_CLIENT(mb_wm_object_new (MB_WM_TYPE_CLIENT_DIALOG,
				     MBWMObjectPropWm,           wm,
				     MBWMObjectPropClientWindow, win,
				     NULL));

  return client;
}

