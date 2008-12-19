#include "mb-wm.h"
#include "mb-wm-client-input.h"

static void
mb_wm_layout_real_update (MBWMLayout * layout);
static void
mb_wm_layout_real_layout_panels (MBWMLayout *layout, MBGeometry * avail_geom);
static void
mb_wm_layout_real_layout_input (MBWMLayout *layout, MBGeometry * avail_geom);
static void
mb_wm_layout_real_layout_free (MBWMLayout *layout, MBGeometry * avail_geom);
static void
mb_wm_layout_real_layout_fullscreen (MBWMLayout *layout, MBGeometry * avail_geom);

static void
mb_wm_layout_class_init (MBWMObjectClass *klass)
{
  MBWMLayoutClass * layout_class = MB_WM_LAYOUT_CLASS (klass);

  layout_class->update = mb_wm_layout_real_update;
  layout_class->layout_panels = mb_wm_layout_real_layout_panels;
  layout_class->layout_input = mb_wm_layout_real_layout_input;
  layout_class->layout_free = mb_wm_layout_real_layout_free;
  layout_class->layout_fullscreen = mb_wm_layout_real_layout_fullscreen;

#if MBWM_WANT_DEBUG
  klass->klass_name = "MBWMLayout";
#endif
}

static void
mb_wm_layout_destroy (MBWMObject *this)
{
}

static int
mb_wm_layout_init (MBWMObject *this, va_list vap)
{
  MBWMLayout *layout = MB_WM_LAYOUT (this);
  MBWMObjectProp    prop;
  MBWindowManager  *wm = NULL;

  prop = va_arg(vap, MBWMObjectProp);
  while (prop)
    {
      switch (prop)
	{
	case MBWMObjectPropWm:
	  wm = va_arg(vap, MBWindowManager *);
	  break;
	default:
	  MBWMO_PROP_EAT (vap, prop);
	}

      prop = va_arg(vap, MBWMObjectProp);
    }

  MBWM_ASSERT (wm);

  layout->wm = wm;

  return 1;
}

int
mb_wm_layout_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (MBWMLayoutClass),
	sizeof (MBWMLayout),
	mb_wm_layout_init,
	mb_wm_layout_destroy,
	mb_wm_layout_class_init
      };

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_OBJECT, 0);
    }

  return type;
}

MBWMLayout*
mb_wm_layout_new (MBWindowManager *wm)
{
  MBWMLayout *layout;

  layout = MB_WM_LAYOUT (mb_wm_object_new (MB_WM_TYPE_LAYOUT,
					   MBWMObjectPropWm, wm,
					   NULL));
  return layout;
}

Bool
mb_wm_layout_clip_geometry (MBGeometry *geom,
			    MBGeometry *min,
			    int         flags)
{
  Bool changed = False;

  if (flags & SET_X)
    if (geom->x < min->x || geom->x > min->x + min->width)
      {
	geom->x = min->x;
	changed = True;
      }

  if (flags & SET_Y)
    if (geom->y < min->y || geom->y > min->y + min->height)
      {
	geom->y = min->y;
	changed = True;
      }

  if (flags & SET_WIDTH)
    if (geom->x + geom->width > min->x + min->width)
      {
	int old_width = geom->width;

	geom->width = min->x + min->width - geom->x;

	/*
	 * if we are about to reduce the width, and are asked also to set the x
	 * coord, see if we can move the window left so it can be bigger.
	 */
	if ((flags & SET_X) && old_width > geom->width && geom->x > min->x)
	  {
	    int w_diff = old_width - geom->width;
	    int x = geom->x - w_diff;
	    int x_diff;

	    if (x < min->x)
	      x = min->x;

	    x_diff = geom->x - x;

	    geom->x = x;
	    geom->width += x_diff;
	  }

	changed = True;
      }

  if (flags & SET_HEIGHT)
    if (geom->y + geom->height > min->y + min->height)
      {
	int old_height = geom->height;

	geom->height = min->y + min->height - geom->y;

	/*
	 * if we are about to reduce the height, and are asked also to set the
	 * y coord, see if we can move the window up so it can be bigger.
	 */
	if ((flags & SET_Y) && old_height > geom->height && geom->y > min->y)
	  {
	    int h_diff = old_height - geom->height;
	    int y = geom->y - h_diff;
	    int y_diff;

	    if (y < min->y)
	      y = min->y;

	    y_diff = geom->y - y;

	    geom->y = y;
	    geom->height += y_diff;
	  }
	changed = True;
      }

  return changed;
}

Bool
mb_wm_layout_maximise_geometry (MBGeometry *geom,
				MBGeometry *max,
				int         flags)
{
  Bool changed = False;

  if (flags & SET_X && geom->x != max->x)
    {
      geom->x = max->x;
      changed = True;
    }

  if (flags & SET_Y && geom->y != max->y)
    {
      geom->y = max->y;
      changed = True;
    }

  if (flags & SET_WIDTH && geom->width != max->width)
    {
      geom->width = max->width;
      changed = True;
    }

  if (flags & SET_HEIGHT && geom->height != max->height)
    {
      geom->height = max->height;
      changed = True;
    }

  return changed;
}

static void
mb_wm_layout_real_layout_panels (MBWMLayout *layout, MBGeometry * avail_geom)
{
  MBWindowManager       *wm = layout->wm;
  MBWindowManagerClient *client;
  MBGeometry             coverage;
  Bool                   need_change;

  /* FIXME: need to enumerate by *age* in case multiple panels ? */
  mb_wm_stack_enumerate(wm, client)
    if ((mb_wm_client_get_layout_hints(client) & LayoutPrefReserveEdgeNorth) &&
	(mb_wm_client_get_layout_hints(client) & LayoutPrefVisible))
      {
	int flags = SET_Y;

	if (!(mb_wm_client_get_layout_hints (client) & LayoutPrefFixedX))
	  flags |= SET_X | SET_WIDTH;

	mb_wm_client_get_coverage (client, &coverage);

	/* set x,y to avail and max width */
	need_change = mb_wm_layout_maximise_geometry (&coverage,
						      avail_geom, flags);
	/* Too high */
	need_change |= mb_wm_layout_clip_geometry (&coverage,
						   avail_geom, SET_HEIGHT);

	if (need_change)
	  mb_wm_client_request_geometry (client,
					 &coverage,
					 MBWMClientReqGeomIsViaLayoutManager);
	  /* FIXME: what if this returns False ? */

	if (!(mb_wm_client_get_layout_hints (client) & LayoutPrefOverlaps))
	  {
	    avail_geom->y      = coverage.y + coverage.height;
	    avail_geom->height = avail_geom->height - coverage.height;
	  }
      }

  mb_wm_stack_enumerate(wm, client)
    if ((mb_wm_client_get_layout_hints(client) & LayoutPrefReserveEdgeSouth) &&
	(mb_wm_client_get_layout_hints(client) & LayoutPrefVisible))
      {
	int y;

	mb_wm_client_get_coverage (client, &coverage);

	/* set x,y to avail and max width */
	if (!(mb_wm_client_get_layout_hints (client) & LayoutPrefFixedX))
	  need_change = mb_wm_layout_maximise_geometry (&coverage,
							avail_geom,
							SET_X | SET_WIDTH);
	else
	  need_change = False;

	y = avail_geom->y + avail_geom->height - coverage.height;

	if (y != coverage.y)
	  {
	    coverage.y = y;
	    need_change = True;
	  }

	/* Too high */
	need_change |= mb_wm_layout_clip_geometry (&coverage,
						     avail_geom, SET_HEIGHT);

	if (need_change)
	  mb_wm_client_request_geometry (client,
					 &coverage,
					 MBWMClientReqGeomIsViaLayoutManager);

	if (!(mb_wm_client_get_layout_hints (client) & LayoutPrefOverlaps))
	  avail_geom->height = avail_geom->height - coverage.height;
      }

  mb_wm_stack_enumerate(wm, client)
    if ((mb_wm_client_get_layout_hints(client) & LayoutPrefReserveEdgeWest) &&
	(mb_wm_client_get_layout_hints(client) & LayoutPrefVisible))
      {
	int flags = SET_X;

	if (!(mb_wm_client_get_layout_hints (client) & LayoutPrefFixedY))
	  flags |= SET_Y | SET_HEIGHT;

	mb_wm_client_get_coverage (client, &coverage);

	/* set x,y to avail and max width */
	need_change = mb_wm_layout_maximise_geometry (&coverage,
						      avail_geom,
						      flags);
	/* Too wide */
	need_change |= mb_wm_layout_clip_geometry (&coverage,
						   avail_geom, SET_WIDTH);

	if (need_change)
	  mb_wm_client_request_geometry (client,
					 &coverage,
					 MBWMClientReqGeomIsViaLayoutManager);

	if (!(mb_wm_client_get_layout_hints (client) & LayoutPrefOverlaps))
	  {
	    avail_geom->x      = coverage.x + coverage.width;
	    avail_geom->width  = avail_geom->width - coverage.width;
	  }
      }


  mb_wm_stack_enumerate(wm, client)
    if ((mb_wm_client_get_layout_hints(client) & LayoutPrefReserveEdgeEast) &&
	(mb_wm_client_get_layout_hints(client) & LayoutPrefVisible))
      {
	int x;

	mb_wm_client_get_coverage (client, &coverage);

	/* set x,y to avail and max width */
	if (!(mb_wm_client_get_layout_hints (client) & LayoutPrefFixedY))
	  need_change = mb_wm_layout_maximise_geometry (&coverage,
							avail_geom,
							SET_Y|SET_HEIGHT);
	else
	  need_change = False;

	x = avail_geom->x + avail_geom->width - coverage.width;

	if (x != coverage.x)
	  {
	    coverage.x = x;
	    need_change = True;
	  }

	/* Too wide */
	need_change |= mb_wm_layout_clip_geometry (&coverage,
						   avail_geom, SET_WIDTH);

	if (need_change)
	  mb_wm_client_request_geometry (client,
					 &coverage,
					 MBWMClientReqGeomIsViaLayoutManager);

	if (!(mb_wm_client_get_layout_hints (client) & LayoutPrefOverlaps))
	  avail_geom->width  = avail_geom->width - coverage.width;
      }
}

static void
mb_wm_layout_real_layout_input (MBWMLayout *layout, MBGeometry * avail_geom)
{
  MBWindowManager       *wm = layout->wm;
  MBWindowManagerClient *client;
  MBGeometry             coverage;
  Bool                   need_change = False;

  mb_wm_stack_enumerate(wm, client)
    if (mb_wm_client_get_layout_hints (client) ==
	(LayoutPrefReserveNorth|LayoutPrefVisible))
      {
	mb_wm_client_get_coverage (client, &coverage);

	/* set x,y to avail and max width */
	need_change = mb_wm_layout_maximise_geometry (&coverage,
						      avail_geom,
						      SET_X|SET_Y|SET_WIDTH);

	if (client->transient_for &&
	    (client->transient_for->window->ewmh_state &
	     MBWMClientWindowEWMHStateFullscreen) &&
	    coverage.width != wm->xdpy_width)
	  {
	    coverage.width = wm->xdpy_width;
	    coverage.x = 0;
	    need_change = True;
	  }

	/* Too high */
	need_change |= mb_wm_layout_clip_geometry (&coverage,
						   avail_geom, SET_HEIGHT);

	if (need_change)
	  mb_wm_client_request_geometry (client,
					 &coverage,
					 MBWMClientReqGeomIsViaLayoutManager);
	  /* FIXME: what if this returns False ? */

	avail_geom->y      = coverage.y + coverage.height;
	avail_geom->height = avail_geom->height - coverage.height;
      }

  mb_wm_stack_enumerate(wm, client)
    if (mb_wm_client_get_layout_hints (client) ==
	(LayoutPrefReserveSouth|LayoutPrefVisible))
      {
	int y;

	mb_wm_client_get_coverage (client, &coverage);

	/* First of all, tweak the y value so that we start with position
	 * that as much as possible respects the request for south edge
	 * for the current geometry. For example, the hildon input method
	 * initially maps with height 1 and y 399; it then requests resize
	 * to some sensible height, but does not adjust the y value.
	 */
	y = avail_geom->y + avail_geom->height - coverage.height;

	if (y < 0)
	  y = 0;

	if (y != coverage.y)
	  need_change = True;

	coverage.y = y;

	/* set x and width */
	need_change |= mb_wm_layout_maximise_geometry (&coverage,
						       avail_geom,
						       SET_X|SET_WIDTH);

	if (client->transient_for &&
	    (client->transient_for->window->ewmh_state &
	     MBWMClientWindowEWMHStateFullscreen) &&
	    coverage.width != wm->xdpy_width)
	  {
	    coverage.width = wm->xdpy_width;
	    coverage.x = 0;
	    need_change = True;
	  }

	/* Too high */
	need_change |= mb_wm_layout_clip_geometry (&coverage,
						   avail_geom, SET_HEIGHT);

	if (coverage.y != avail_geom->y + avail_geom->height - coverage.height)
	  {
	    coverage.y = avail_geom->y + avail_geom->height - coverage.height;
	    need_change = True;
	  }

	if (need_change)
	  mb_wm_client_request_geometry (client,
					 &coverage,
					 MBWMClientReqGeomIsViaLayoutManager);

	avail_geom->height = avail_geom->height - coverage.height;
      }


  mb_wm_stack_enumerate(wm, client)
    if (mb_wm_client_get_layout_hints (client) ==
	(LayoutPrefReserveWest|LayoutPrefVisible))
      {
	mb_wm_client_get_coverage (client, &coverage);

	/* set x,y to avail and max width */
	need_change = mb_wm_layout_maximise_geometry (&coverage,
						      avail_geom,
						      SET_X|SET_Y|SET_HEIGHT);

	if (client->transient_for &&
	    (client->transient_for->window->ewmh_state &
	     MBWMClientWindowEWMHStateFullscreen) &&
	    coverage.height != wm->xdpy_height)
	  {
	    coverage.height = wm->xdpy_height;
	    coverage.y = 0;
	    need_change = True;
	  }

	/* Too wide */
	need_change |= mb_wm_layout_clip_geometry (&coverage,
						   avail_geom, SET_WIDTH);

	if (need_change)
	  mb_wm_client_request_geometry (client,
					 &coverage,
					 MBWMClientReqGeomIsViaLayoutManager);

	avail_geom->x      = coverage.x + coverage.width;
	avail_geom->width  = avail_geom->width - coverage.width;
      }


  mb_wm_stack_enumerate(wm, client)
    if (mb_wm_client_get_layout_hints (client) ==
	(LayoutPrefReserveEast|LayoutPrefVisible))
      {
	mb_wm_client_get_coverage (client, &coverage);

	/* set x,y to avail and max width */
	need_change = mb_wm_layout_maximise_geometry (&coverage,
						      avail_geom,
						      SET_Y|SET_HEIGHT);

	if (client->transient_for &&
	    (client->transient_for->window->ewmh_state &
	     MBWMClientWindowEWMHStateFullscreen) &&
	    coverage.height != wm->xdpy_height)
	  {
	    coverage.height = wm->xdpy_height;
	    coverage.y = 0;
	    need_change = True;
	  }

	/* Too wide */
	need_change |= mb_wm_layout_clip_geometry (&coverage,
						   avail_geom, SET_WIDTH);

	if (need_change)
	  mb_wm_client_request_geometry (client,
					 &coverage,
					 MBWMClientReqGeomIsViaLayoutManager);

	if (coverage.x != avail_geom->x + avail_geom->width - coverage.width)
	  {
	    coverage.x = avail_geom->x + avail_geom->width - coverage.width;
	    need_change = True;
	  }

	avail_geom->width  = avail_geom->width - coverage.width;
      }
}

static void
mb_wm_layout_real_layout_free (MBWMLayout *layout, MBGeometry * avail_geom)
{
  MBWindowManager       *wm = layout->wm;
  MBWindowManagerClient *client;
  MBGeometry             coverage;
  Bool                   need_change;

  mb_wm_stack_enumerate(wm, client)
    if (mb_wm_client_get_layout_hints (client) ==
	(LayoutPrefGrowToFreeSpace|LayoutPrefVisible))
      {
	mb_wm_client_get_coverage (client, &coverage);

	if (coverage.x != avail_geom->x
	    || coverage.width != avail_geom->width
	    || coverage.y != avail_geom->y
	    || coverage.height != avail_geom->height)
	  {
	    coverage.width  = avail_geom->width;
	    coverage.height = avail_geom->height;
	    coverage.x      = avail_geom->x;
	    coverage.y      = avail_geom->y;

	    mb_wm_client_request_geometry (client,
					 &coverage,
					 MBWMClientReqGeomIsViaLayoutManager);
	  }
      }

  mb_wm_stack_enumerate(wm, client)
    {
      MBWMClientLayoutHints hints = mb_wm_client_get_layout_hints (client);

    if ((hints & LayoutPrefPositionFree) && (hints & LayoutPrefVisible) &&
	!(hints & (LayoutPrefFixedX|LayoutPrefFixedY)))
      {
	/* Clip if needed */
	mb_wm_client_get_coverage (client, &coverage);

	need_change = mb_wm_layout_clip_geometry (&coverage,
						  avail_geom,
						  SET_X | SET_Y |
						  SET_HEIGHT | SET_WIDTH);

	if (need_change)
	  mb_wm_client_request_geometry (client,
					 &coverage,
					 MBWMClientReqGeomIsViaLayoutManager);
	}
      }

}

static void
mb_wm_layout_real_layout_fullscreen (MBWMLayout *layout, MBGeometry * avail_geom)
{
  MBWindowManager       *wm = layout->wm;
  MBWindowManagerClient *client;
  MBGeometry             coverage;

  mb_wm_stack_enumerate(wm, client)
    if (mb_wm_client_get_layout_hints (client) ==
	(LayoutPrefFullscreen|LayoutPrefVisible))
      {
	MBWMList *l = mb_wm_client_get_transients (client);

	mb_wm_client_get_coverage (client, &coverage);

	/* See if this client comes with an input method and if so,
	 * adjust the available geometry accordingly
	 */
	while (l)
	  {
	    MBWindowManagerClient * c = l->data;

	    if (MB_WM_CLIENT_CLIENT_TYPE (c) == MBWMClientTypeInput)
	      {
		MBGeometry geom;
		mb_wm_client_get_coverage (c, &geom);

		if (mb_wm_client_get_layout_hints (c) ==
		    (LayoutPrefReserveSouth|LayoutPrefVisible))
		  {
		    if (geom.y < avail_geom->y + avail_geom->height)
		      {
			avail_geom->height = geom.y - avail_geom->y;
		      }
		  }
		else if (mb_wm_client_get_layout_hints (c) ==
		    (LayoutPrefReserveNorth|LayoutPrefVisible))
		  {
		    if (geom.height && geom.height + geom.y > avail_geom->y)
		      {
			int y = avail_geom->y;

			avail_geom->y = geom.y + geom.height;
			avail_geom->height -= y - avail_geom->y;
		      }
		  }
		else if (mb_wm_client_get_layout_hints (c) ==
		    (LayoutPrefReserveWest|LayoutPrefVisible))
		  {
		    if (geom.x < avail_geom->x + avail_geom->width)
		      {
			avail_geom->width = geom.x - avail_geom->x;
		      }
		  }
		else if (mb_wm_client_get_layout_hints (c) ==
		    (LayoutPrefReserveEast|LayoutPrefVisible))
		  {
		    if (geom.width && geom.width + geom.x > avail_geom->x)
		      {
			int x = avail_geom->x;

			avail_geom->x = geom.x + geom.width;
			avail_geom->width -= x - avail_geom->x;
		      }
		  }

		break;
	      }

	    l = l->next;
	  }

	if (coverage.x != avail_geom->x
	    || coverage.width != avail_geom->width
	    || coverage.y != avail_geom->y
	    || coverage.height != avail_geom->height)
	  {
	    coverage.width  = avail_geom->width;
	    coverage.height = avail_geom->height;
	    coverage.x      = avail_geom->x;
	    coverage.y      = avail_geom->y;

	    mb_wm_client_request_geometry (client,
					 &coverage,
					 MBWMClientReqGeomIsViaLayoutManager);
	  }

	mb_wm_util_list_free (l);
      }
}

static void
mb_wm_layout_real_update (MBWMLayout * layout)
{
  MBWMLayoutClass       *klass;
  MBWindowManager       *wm = layout->wm;
  MBGeometry             avail_geom;

  klass = MB_WM_LAYOUT_CLASS (MB_WM_OBJECT_GET_CLASS (layout));

  MBWM_ASSERT (klass->layout_panels);
  MBWM_ASSERT (klass->layout_input);
  MBWM_ASSERT (klass->layout_free);
  MBWM_ASSERT (klass->layout_fullscreen);

  mb_wm_get_display_geometry (wm, &avail_geom);

  /*
    cycle through clients, laying out each in below order.
    Note they must have LayoutPrefVisible set.

    LayoutPrefReserveEdgeNorth
    LayoutPrefReserveEdgeSouth

    LayoutPrefReserveEdgeEast
    LayoutPrefReserveEdgeWest

    LayoutPrefReserveNorth
    LayoutPrefReserveSouth

    LayoutPrefReserveEast
    LayoutPrefReserveWest

    LayoutPrefGrowToFreeSpace

    LayoutPrefFullscreen

    XXX need to check they are mapped too

    foreach client with LayoutPrefReserveEdgeNorth & LayoutPrefVisible
       grab there current geometry
          does it fit well into current restraints ( min_, max_ )
            yes leave
            no  resize so it does, mark dirty
          set min_x, max_y, min_y, max_y to current size
    repeat for next condition

    mb_wm_client_get_coverage (MBWindowManagerClient *client,
    MBGeometry            *coverage)

 */

  klass->layout_panels (layout, &avail_geom);
  klass->layout_input  (layout, &avail_geom);
  klass->layout_free   (layout, &avail_geom);

  mb_wm_get_display_geometry (wm, &avail_geom);
  klass->layout_fullscreen (layout, &avail_geom);
}

void
mb_wm_layout_update (MBWMLayout * layout)
{
  MBWMLayoutClass *klass;

  klass = MB_WM_LAYOUT_CLASS (MB_WM_OBJECT_GET_CLASS (layout));

  MBWM_ASSERT (klass->update);

  klass->update (layout);
}
