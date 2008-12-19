#include "mb-wm.h"
#include "xas.h"

MBWMCookie
mb_wm_property_req (MBWindowManager *wm,
		    Window           win,
		    Atom             property,
		    long             offset,
		    long             length,
		    Bool             delete,
		    Atom             req_type)
{
  XasCookie cookie;

  cookie = xas_get_property(wm->xas_context,
			    win,
			    property,
			    offset,
			    length,
			    delete,
			    req_type);

  return (MBWMCookie)cookie;
}


Status
mb_wm_property_reply (MBWindowManager  *wm,
		      MBWMCookie        cookie,
		      Atom             *actual_type_return,
		      int              *actual_format_return,
		      unsigned long    *nitems_return,
		      unsigned long    *bytes_after_return,
		      unsigned char   **prop_return,
		      int              *x_error_code)
{
  return  xas_get_property_reply(wm->xas_context,
			         (XasCookie)cookie,
				 actual_type_return,
				 actual_format_return,
				 nitems_return,
				 bytes_after_return,
				 prop_return,
				 x_error_code);
}

void*
mb_wm_property_get_reply_and_validate (MBWindowManager  *wm,
				       MBWMCookie        cookie,
				       Atom              expected_type,
				       int               expected_format,
				       int               expected_n_items,
				       int              *n_items_ret,
				       int              *x_error_code)
{
  Atom             actual_type_return;
  int              actual_format_return;
  unsigned long    nitems_return;
  unsigned long    bytes_after_return;
  unsigned char   *prop_data = NULL;

  *x_error_code = 0;

  xas_get_property_reply(wm->xas_context,
			 (XasCookie)cookie,
			 &actual_type_return,
			 &actual_format_return,
			 &nitems_return,
			 &bytes_after_return,
			 &prop_data,
			 x_error_code);

  if (*x_error_code || prop_data == NULL)
    goto fail;

  if (expected_format && actual_format_return != expected_format)
    goto fail;

  if (expected_n_items && nitems_return != expected_n_items)
    goto fail;

  if (n_items_ret)
    *n_items_ret = nitems_return;

  return prop_data;

 fail:

  if (prop_data)
    XFree(prop_data);

  return NULL;
}



Bool
mb_wm_property_have_reply (MBWindowManager     *wm,
			   MBWMCookie           cookie)
{
  return xas_have_reply(wm->xas_context, (XasCookie)cookie);
}


MBWMCookie
mb_wm_xwin_get_attributes (MBWindowManager   *wm,
			   Window             win)
{
  return xas_get_window_attributes(wm->xas_context, win);
}

MBWMCookie
mb_wm_xwin_get_geometry (MBWindowManager   *wm,
			 Drawable            d)
{
  return xas_get_geometry(wm->xas_context, d);
}

MBWMClientWindowAttributes*
mb_wm_xwin_get_attributes_reply (MBWindowManager   *wm,
				 MBWMCookie         cookie,
				 int               *x_error_code)
{
  return (MBWMClientWindowAttributes*)
    xas_get_window_attributes_reply(wm->xas_context,
				    cookie,
				    x_error_code);
}

Status
mb_wm_xwin_get_geometry_reply (MBWindowManager   *wm,
			       XasCookie          cookie,
			       MBGeometry        *geom_return,
			       unsigned int      *border_width_return,
			       unsigned int      *depth_return,
			       int               *x_error_code)
{
  return xas_get_geometry_reply (wm->xas_context,
				 cookie,
				 &geom_return->x,
				 &geom_return->y,
				 &geom_return->width,
				 &geom_return->height,
				 border_width_return,
				 depth_return,
				 x_error_code);
}


void
mb_wm_props_send_x_message (MBWindowManager *wm,
			    Window           xwin_src,
			    Window           xwin_dest,
			    Atom             delivery_atom,
			    unsigned long    data0,
			    unsigned long    data1,
			    unsigned long    data2,
			    unsigned long    data3,
			    unsigned long    data4,
			    unsigned long    mask)
{
  XEvent ev;

  memset(&ev, 0, sizeof(ev));

  ev.xclient.type = ClientMessage;
  ev.xclient.window = xwin_src;
  ev.xclient.message_type = delivery_atom;
  ev.xclient.format = 32;
  ev.xclient.data.l[0] = data0;
  ev.xclient.data.l[1] = data1;
  ev.xclient.data.l[2] = data2;
  ev.xclient.data.l[3] = data3;
  ev.xclient.data.l[4] = data4;

  if (!mask)
    mask = NoEventMask;

  /* FIXME: traps */

  XSendEvent(wm->xdpy, xwin_dest, False, mask, &ev);
  XSync(wm->xdpy, False);

}

void
mb_wm_props_sync_root_props (MBWindowManager *wm)
{




}

void
mb_wm_props_root_message (MBWindowManager *wm)
{




}

void
mb_wm_rename_window (MBWindowManager *wm,
		Window win,
		const char *name)
{
	XChangeProperty(wm->xdpy, win,
			wm->atoms[MBWM_ATOM_NET_WM_NAME],
			wm->atoms[MBWM_ATOM_UTF8_STRING],
			8, PropModeReplace,
			(unsigned char *)name, strlen(name)+1);

	XStoreName(wm->xdpy, win, name);
}


int
mb_window_is_secondary (MBWindowManager *wm, Window w)
{
  Atom actual_type_return;
  int actual_format_return;
  unsigned long nitems_return;
  unsigned long bytes_after_return;
  unsigned char* prop_return = NULL;

  XGetWindowProperty (wm->xdpy, w,
		      wm->atoms[MBWM_ATOM_MB_SECONDARY],
		      0, 0,
		      False,
		      AnyPropertyType,
		      &actual_type_return,
		      &actual_format_return,
		      &nitems_return,
		      &bytes_after_return,
		      &prop_return);

  if (prop_return)
    XFree (prop_return);

  return (actual_type_return!=None);
}
