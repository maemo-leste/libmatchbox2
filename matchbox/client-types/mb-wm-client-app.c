#include "mb-wm-client-app.h"

#include "mb-wm-theme.h"

static Bool
mb_wm_client_app_request_geometry (MBWindowManagerClient *client,
				   MBGeometry            *new_geometry,
				   MBWMClientReqGeomType  flags);

static void
mb_wm_client_app_theme_change (MBWindowManagerClient *client);

static void
mb_wm_client_app_class_init (MBWMObjectClass *klass)
{
  MBWindowManagerClientClass *client;
  MBWMClientAppClass * client_app;

  MBWM_MARK();

  client     = (MBWindowManagerClientClass *)klass;
  client_app = (MBWMClientAppClass *)klass;

  MBWM_DBG("client->stack is %p", client->stack);

  client->client_type = MBWMClientTypeApp;
  client->geometry = mb_wm_client_app_request_geometry;
  client->theme_change = mb_wm_client_app_theme_change;

#if MBWM_WANT_DEBUG
  klass->klass_name = "MBWMClientApp";
#endif
}

static void
mb_wm_client_app_destroy (MBWMObject *this)
{
}


static int
mb_wm_client_app_init (MBWMObject *this, va_list vap)
{
  MBWindowManagerClient    *client     = MB_WM_CLIENT (this);
  MBWindowManager          *wm = NULL;
  MBWMClientAppClass       *app_class;

  app_class = MB_WM_CLIENT_APP_CLASS (MB_WM_OBJECT_GET_CLASS (this));

#if 0
  /*
   * Property parsing not needed for now, as there are no ClientApp specific
   * properties
   */
  prop = va_arg(vap, MBWMObjectProp);
  while (prop)
    {
      if (prop == MBWMObjectPropWm)
	{
	  wm = va_arg(vap, MBWindowManager *);
	  break;
	}
      else
	MBWMO_PROP_EAT (vap, prop);

      prop = va_arg (vap, MBWMObjectProp);
    }
#endif

  wm = client->wmref;

  if (!wm)
    return 0;

  {
    Atom actions[] = {
      wm->atoms[MBWM_ATOM_NET_WM_ACTION_CLOSE],
      wm->atoms[MBWM_ATOM_NET_WM_ACTION_FULLSCREEN]
    };

    XChangeProperty (wm->xdpy, client->window->xwindow,
		     wm->atoms[MBWM_ATOM_NET_WM_ALLOWED_ACTIONS],
		     XA_ATOM, 32, PropModeReplace,
		     (unsigned char *)actions,
		     sizeof (actions)/sizeof (actions[0]));
  }

  if (client->window->hildon_stacking_layer == 0)
    client->stacking_layer = MBWMStackLayerMid;
  else
    client->stacking_layer = client->window->hildon_stacking_layer
	                     + MBWMStackLayerHildon1 - 1;

  mb_wm_client_set_layout_hints (client,
				 LayoutPrefGrowToFreeSpace|LayoutPrefVisible);

  if (!client->window->undecorated)
    {
      mb_wm_theme_create_decor (wm->theme, client, MBWMDecorTypeNorth);
      mb_wm_theme_create_decor (wm->theme, client, MBWMDecorTypeSouth);
      mb_wm_theme_create_decor (wm->theme, client, MBWMDecorTypeWest);
      mb_wm_theme_create_decor (wm->theme, client, MBWMDecorTypeEast);
    }

  return 1;
}

int
mb_wm_client_app_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (MBWMClientAppClass),
	sizeof (MBWMClientApp),
	mb_wm_client_app_init,
	mb_wm_client_app_destroy,
	mb_wm_client_app_class_init
      };
      type = mb_wm_object_register_class (&info, MB_WM_TYPE_CLIENT_BASE, 0);
    }

  return type;
}

static Bool
mb_wm_client_app_request_geometry (MBWindowManagerClient *client,
				   MBGeometry            *new_geometry,
				   MBWMClientReqGeomType  flags)
{
  if (flags & (MBWMClientReqGeomIsViaLayoutManager |
	       MBWMClientReqGeomForced             |
	       MBWMClientReqGeomIsViaUserAction))
    {
      int north, south, west, east;
      MBWindowManager *wm = client->wmref;

      if ((client->window->ewmh_state & MBWMClientWindowEWMHStateFullscreen)||
	  !client->decor)
	{
	  /* Undecorated window */
	  client->window->geometry.x      = new_geometry->x;
	  client->window->geometry.y      = new_geometry->y;
	  client->window->geometry.width  = new_geometry->width;
	  client->window->geometry.height = new_geometry->height;

	  client->frame_geometry.x        = new_geometry->x;
	  client->frame_geometry.y        = new_geometry->y;
	  client->frame_geometry.width    = new_geometry->width;
	  client->frame_geometry.height   = new_geometry->height;
	}
      else
	{
	  mb_wm_theme_get_decor_dimensions (wm->theme, client,
					    &north, &south, &west, &east);

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
  
  return False;
}

static void
mb_wm_client_app_theme_change (MBWindowManagerClient *client)
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
mb_wm_client_app_new (MBWindowManager *wm, MBWMClientWindow *win)
{
  MBWindowManagerClient *client;

  client = MB_WM_CLIENT(mb_wm_object_new (MB_WM_TYPE_CLIENT_APP,
					  MBWMObjectPropWm,           wm,
					  MBWMObjectPropClientWindow, win,
					  NULL));

  return client;
}

