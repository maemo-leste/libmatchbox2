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

static void mb_wm_decor_button_unrealize (MBWMDecorButton *button);
static void mb_wm_decor_button_realize (MBWMDecorButton *button);


static void
mb_wm_decor_destroy (MBWMObject *obj);

static void
mb_wm_decor_button_sync_window (MBWMDecorButton *button);

static void
mb_wm_decor_class_init (MBWMObjectClass *klass)
{
#if MBWM_WANT_DEBUG
  klass->klass_name = "MBWMDecor";
#endif
}

static int
mb_wm_decor_init (MBWMObject *obj, va_list vap)
{
  MBWMDecor             *decor = MB_WM_DECOR (obj);
  MBWindowManager       *wm = NULL;
  MBWMDecorType          type = 0;
  MBWMObjectProp         prop;
  int                    abs_packing = 0;

  prop = va_arg(vap, MBWMObjectProp);
  while (prop)
    {
      switch (prop)
	{
	case MBWMObjectPropWm:
	  wm = va_arg(vap, MBWindowManager *);
	  break;
	case MBWMObjectPropDecorType:
	  type = va_arg(vap, MBWMDecorType);
	  break;
	case MBWMObjectPropDecorAbsolutePacking:
	  abs_packing = va_arg(vap, int);
          break;
	default:
	  MBWMO_PROP_EAT (vap, prop);
	}

      prop = va_arg(vap, MBWMObjectProp);
    }

  if (!wm)
    return 0;

  decor->type     = type;
  decor->dirty    = MBWMDecorDirtyFull; /* Needs painting */
  decor->absolute_packing = abs_packing;

  return 1;
}

int
mb_wm_decor_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (MBWMDecorClass),
	sizeof (MBWMDecor),
	mb_wm_decor_init,
	mb_wm_decor_destroy,
	mb_wm_decor_class_init
      };

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_OBJECT, 0);
    }

  return type;
}

static void
mb_wm_decor_repaint (MBWMDecor *decor)
{
  MBWMTheme   *theme = decor->parent_client->wmref->theme;

  mb_wm_theme_paint_decor (theme, decor);
}

static void
mb_wm_decor_resize (MBWMDecor *decor)
{
  const MBGeometry *geom;
  MBWMTheme        *theme = decor->parent_client->wmref->theme;
  MBWMList         *l;
  int               btn_x_start, btn_x_end;
  int               abs_packing = decor->absolute_packing;

  geom = mb_wm_decor_get_geometry (decor);

  btn_x_end = geom->width;
  btn_x_start = 0;

  l = decor->buttons;

  /*
   * Notify theme of resize
   */
  mb_wm_theme_resize_decor (theme, decor);

  if (abs_packing)
    {
      int width = btn_x_end;

      width /= 2;

      while (l)
	{
	  int off_x, off_y, bw, bh;

	  MBWMDecorButton  *btn = (MBWMDecorButton  *)l->data;
	  mb_wm_theme_get_button_position (theme, decor, btn->type,
					   &off_x, &off_y);
	  mb_wm_theme_get_button_size (theme, decor, btn->type,
				       &bw, &bh);

	  mb_wm_decor_button_move_to (btn, off_x, off_y);

          /*
           * We need to simulate packing when placing buttons at absolute
           * positions (e.g., in png-based theme) so that we know the size
           * of the area into which we can place the document title
           */
	  if (off_x + bw < width)
	    {
	      int x = off_x + bw;

	      if (x > btn_x_start)
		btn_x_start = x + 2;
	    }
	  else
	    {
	      if (off_x < btn_x_end)
		btn_x_end = off_x - 2;
	    }

	  l = l->next;
	}
    }
  else
    {
      while (l)
	{
	  int off_x, off_y;

	  MBWMDecorButton  *btn = (MBWMDecorButton  *)l->data;
	  mb_wm_theme_get_button_position (theme, decor, btn->type,
					   &off_x, &off_y);

	  if (btn->pack == MBWMDecorButtonPackEnd)
	    {
	      btn_x_end -= (btn->geom.width + off_x);
	      mb_wm_decor_button_move_to (btn, btn_x_end, off_y);
	    }
	  else
	    {
	      mb_wm_decor_button_move_to (btn, btn_x_start + off_x, off_y);
	      btn_x_start += btn->geom.width;
	    }

	  l = l->next;
	}
    }

  decor->pack_start_x = btn_x_start;
  decor->pack_end_x   = btn_x_end;
}

static Bool
mb_wm_decor_reparent (MBWMDecor *decor);

static Bool
mb_wm_decor_release_handler (XButtonEvent    *xev,
			     void            *userdata)
{
  MBWMDecor       *decor  = userdata;
  MBWindowManager *wm;
 
  if (decor == NULL || decor->parent_client == NULL
      || decor->parent_client->wmref == NULL)
    return False;

  wm = decor->parent_client->wmref;

  mb_wm_main_context_x_event_handler_remove (wm->main_ctx, ButtonRelease,
					     decor->release_cb_id);

  decor->release_cb_id = 0;

  XUngrabPointer (wm->xdpy, CurrentTime);

  return False;
}

static Bool
mb_wm_decor_press_handler (XButtonEvent    *xev,
			   void            *userdata)
{
  MBWMDecor       *decor  = userdata;
  MBWindowManager *wm = decor->parent_client->wmref;
  Bool             retval = True;

  if (xev->window == decor->xwin)
    {
      XEvent     ev;
      MBGeometry geom;
      int        orig_x, orig_y;
      int        orig_p_x, orig_p_y;

      mb_wm_client_get_coverage (decor->parent_client, &geom);

      orig_x   = geom.x;
      orig_y   = geom.y;
      orig_p_x = xev->x_root;
      orig_p_y = xev->y_root;

      /*
       * This is bit tricky: we do a normal pointer drag and pull out any
       * pointer events out of the queue; when we get a MotionEvent, we
       * move the client window. However, for the move to propagete on screen
       * (particularly with a compositor) we need to spin the main loop so
       * that any queued up ConfigureNotify events get processed;
       * unfortunately, this invariably results in the ButtonRelease event
       * landing in the main loop and not in our sub-loop here. So, on the
       * ButtonPress we install a ButtonRelease callback into the main loop
       * and use that to release the grab.
       */
      if (XGrabPointer(wm->xdpy, xev->subwindow, False,
		       ButtonPressMask|ButtonReleaseMask|
		       PointerMotionMask|EnterWindowMask|LeaveWindowMask,
		       GrabModeAsync,
		       GrabModeAsync,
		       None, None, CurrentTime) == GrabSuccess)
	{

	  decor->release_cb_id = mb_wm_main_context_x_event_handler_add (
				 wm->main_ctx,
			         decor->xwin,
			         ButtonRelease,
			         (MBWMXEventFunc)mb_wm_decor_release_handler,
			         decor);

	  for (;;)
	    {
              if (!decor->release_cb_id)
                {
                  /* the handler was called while we spinned the loop */
		  return False;
                }

	      XMaskEvent(wm->xdpy,
			 ButtonPressMask|ButtonReleaseMask|
			 PointerMotionMask|EnterWindowMask|
			 LeaveWindowMask,
			 &ev);

	      switch (ev.type)
		{
		case MotionNotify:
		  {
		    Bool events_pending;
		    int  event_count = 5; /*Limit how much we spin the loop*/
		    XMotionEvent *pev = (XMotionEvent*)&ev;
		    int diff_x = pev->x_root - orig_p_x;
		    int diff_y = pev->y_root - orig_p_y;

		    geom.x = orig_x + diff_x;
		    geom.y = orig_y + diff_y;

		    mb_wm_client_request_geometry (decor->parent_client,
					   &geom,
					   MBWMClientReqGeomIsViaUserAction);

		    do
		      {
			events_pending =
			  mb_wm_main_context_spin_loop (wm->main_ctx);

			--event_count;
		      } while (events_pending && event_count);
		  }
		  break;
		case ButtonRelease:
		  {
                    mb_wm_main_context_x_event_handler_remove (
                                   wm->main_ctx, ButtonRelease,
			           decor->release_cb_id);
                    decor->release_cb_id = 0;
		    XUngrabPointer (wm->xdpy, CurrentTime);
		    return False;
		  }
		default:
		  ;
		}
	    }
	}
    }

  return retval;
}

static Bool
mb_wm_decor_sync_window (MBWMDecor *decor)
{
  MBWindowManager     *wm;
  XSetWindowAttributes attr;

  if (decor->parent_client == NULL)
    return False;

  wm = decor->parent_client->wmref;


  if (decor->xwin == None)
    {
      if (!decor->geom.width || !decor->geom.height)
        /* Don't bother creating 1-dimension windows, X would reject it
         * anyway, but we wouldn't notice the error. */
        return False;

      attr.override_redirect = True;
      /*attr.background_pixel  = WhitePixel(wm->xdpy, wm->xscreen);*/
      attr.background_pixmap = None;
      attr.event_mask = ButtonPressMask|ButtonReleaseMask|ButtonMotionMask;

      mb_wm_util_trap_x_errors();

      decor->xwin
	= XCreateWindow(wm->xdpy,
			wm->root_win->xwindow,
			decor->geom.x,
			decor->geom.y,
			decor->geom.width,
			decor->geom.height,
			0,
			CopyFromParent,
			CopyFromParent,
			CopyFromParent,
			CWOverrideRedirect/*|CWBackPixel*/|CWEventMask,
			&attr);
      mb_wm_rename_window (wm, decor->xwin, "decor");

      MBWM_DBG("g is +%i+%i %ix%i",
	       decor->geom.x,
	       decor->geom.y,
	       decor->geom.width,
	       decor->geom.height);

      if (mb_wm_util_untrap_x_errors())
	return False;

      mb_wm_decor_resize(decor);

      mb_wm_util_list_foreach(decor->buttons,
		            (MBWMListForEachCB)mb_wm_decor_button_sync_window,
			    NULL);

      /*
       * If this is a decor with buttons, then we install button press handler
       * so we can drag the window, if it is movable.
       */
      if (decor->type == MBWMDecorTypeNorth &&
	  decor->parent_client->layout_hints & LayoutPrefMovable)
	{
                /*
          g_debug ("%s: add ButtonPress handler for %lx", __FUNCTION__,
                   decor->xwin);
                   */
	  decor->press_cb_id =
	    mb_wm_main_context_x_event_handler_add (wm->main_ctx,
			        decor->xwin,
			        ButtonPress,
			        (MBWMXEventFunc)mb_wm_decor_press_handler,
			        decor);
	}

      return mb_wm_decor_reparent (decor);
    }
  else
    {
      /* Resize */
      mb_wm_util_trap_x_errors();

      XMoveResizeWindow(wm->xdpy,
			decor->xwin,
			decor->geom.x,
			decor->geom.y,
			decor->geom.width,
			decor->geom.height);

      /* Next up sort buttons */

      mb_wm_util_list_foreach(decor->buttons,
			      (MBWMListForEachCB)mb_wm_decor_button_sync_window,
			      NULL);

      if (mb_wm_util_untrap_x_errors())
	return False;
    }

  return True;
}

static Bool
mb_wm_decor_reparent (MBWMDecor *decor)
{
  MBWindowManager *wm;

  if (decor->parent_client == NULL)
    return False;

  MBWM_MARK();

  wm = decor->parent_client->wmref;

  /* FIXME: Check if we already have a parent here */

  mb_wm_util_trap_x_errors();

  XReparentWindow (wm->xdpy,
		   decor->xwin,
		   decor->parent_client->xwin_frame,
		   decor->geom.x,
		   decor->geom.y);

  if (mb_wm_util_untrap_x_errors())
    return False;

  return True;
}

static void
mb_wm_decor_calc_geometry (MBWMDecor *decor)
{
  MBWindowManager       *wm;
  MBWindowManagerClient *client;
  int n, s, w, e;

  if (decor->parent_client == NULL)
    return;

  client = decor->parent_client;
  wm = client->wmref;

  mb_wm_theme_get_decor_dimensions (wm->theme, client,
				    &n, &s, &w, &e);

  switch (decor->type)
    {
    case MBWMDecorTypeNorth:
      decor->geom.x      = 0;
      decor->geom.y      = 0;
      decor->geom.height = n;
      decor->geom.width  = client->frame_geometry.width;
      break;
    case MBWMDecorTypeSouth:
      decor->geom.x      = 0;
      decor->geom.y      = client->window->geometry.height + n;
      decor->geom.height = s;
      decor->geom.width  = client->frame_geometry.width;
      break;
    case MBWMDecorTypeWest:
      decor->geom.x      = 0;
      decor->geom.y      = n;
      decor->geom.height = client->window->geometry.height;
      decor->geom.width  = w;
      break;
    case MBWMDecorTypeEast:
      decor->geom.x      = client->window->geometry.width + w;
      decor->geom.y      = n;
      decor->geom.height = client->window->geometry.height;
      decor->geom.width  = e;
      break;
    default:
      /* FIXME: some kind of callback for custom types here ? */
      break;
    }

  MBWM_DBG("geom is +%i+%i %ix%i, Type %i",
	   decor->geom.x,
	   decor->geom.y,
	   decor->geom.width,
	   decor->geom.height,
	   decor->type);

  return;
}

void
mb_wm_decor_handle_map (MBWMDecor *decor)
{
  /* Not needed as XMapSubWindows() is used */
}


void
mb_wm_decor_handle_repaint (MBWMDecor *decor)
{
  MBWMList *l;

  if (decor->parent_client == NULL)
    return;

  if (decor->dirty)
    {
      mb_wm_decor_repaint(decor);

      l = decor->buttons;
      while (l)
	{
	  MBWMDecorButton * button = l->data;
	  mb_wm_decor_button_handle_repaint (button);

	  l = l->next;
	}

      decor->dirty = MBWMDecorDirtyNot;
    }
}

void
mb_wm_decor_handle_resize (MBWMDecor *decor)
{
  if (decor->parent_client == NULL)
    return;

  mb_wm_decor_calc_geometry (decor);

  mb_wm_decor_sync_window (decor);

  /* Fire resize callback */
  mb_wm_decor_resize(decor);

  /* Fire repaint callback */
  mb_wm_decor_mark_dirty (decor);
}

MBWMDecor*
mb_wm_decor_new (MBWindowManager      *wm,
		 MBWMDecorType         type)
{
  MBWMObject *decor;

  decor = mb_wm_object_new (MB_WM_TYPE_DECOR,
			    MBWMObjectPropWm,               wm,
			    MBWMObjectPropDecorType,        type,
			    NULL);

  return MB_WM_DECOR(decor);
}

Window
mb_wm_decor_get_x_window (MBWMDecor *decor)
{
  return decor->xwin;
}

MBWMDecorType
mb_wm_decor_get_type (MBWMDecor *decor)
{
  return decor->type;
}

MBWindowManagerClient*
mb_wm_decor_get_parent (MBWMDecor *decor)
{
  return decor->parent_client;
}


const MBGeometry*
mb_wm_decor_get_geometry (MBWMDecor *decor)
{
  return &decor->geom;
}

int
mb_wm_decor_get_pack_start_x (MBWMDecor *decor)
{
  return decor->pack_start_x;
}


int
mb_wm_decor_get_pack_end_x (MBWMDecor *decor)
{
  return decor->pack_end_x;
}


/* Mark a client in need of a repaint */
void
mb_wm_decor_mark_dirty (MBWMDecor *decor)
{
  decor->dirty |= MBWMDecorDirtyPaint;

  if (decor->parent_client)
    mb_wm_client_decor_mark_dirty (decor->parent_client);
}

void
mb_wm_decor_mark_title_dirty (MBWMDecor *decor)
{
  decor->dirty |= MBWMDecorDirtyTitle;

  if (decor->parent_client)
    mb_wm_client_decor_mark_dirty (decor->parent_client);
}

MBWMDecorDirtyState
mb_wm_decor_get_dirty_state (MBWMDecor *decor)
{
  return decor->dirty;
}

void
mb_wm_decor_attach (MBWMDecor             *decor,
		    MBWindowManagerClient *client)
{
  decor->parent_client = client;
  client->decor = mb_wm_util_list_append(client->decor, decor);

  mb_wm_decor_mark_dirty (decor);

  return;
}

void
mb_wm_decor_detach (MBWMDecor *decor)
{
}

static void
mb_wm_decor_destroy (MBWMObject* obj)
{
  MBWMDecor       * decor = MB_WM_DECOR(obj);
  MBWMList        * l;
  MBWMMainContext * ctx   = decor->parent_client->wmref->main_ctx;

  if (decor->themedata && decor->destroy_themedata)
    {
      decor->destroy_themedata (decor, decor->themedata);
      decor->themedata = NULL;
      decor->destroy_themedata = NULL;
    }

  mb_wm_decor_detach (decor);

  for (l = decor->buttons; l; l = l->next)
    {
      mb_wm_decor_button_unrealize((MBWMDecorButton *)l->data);
      mb_wm_object_unref (MB_WM_OBJECT (l->data));
      /* we don't free, because the dispose handler of the decor button
       * removes itself */
    }

  if (decor->buttons)
    mb_wm_util_list_free (decor->buttons);

  if (decor->press_cb_id)
    mb_wm_main_context_x_event_handler_remove (ctx, ButtonPress,
					       decor->press_cb_id);
  
  if (decor->release_cb_id)
    mb_wm_main_context_x_event_handler_remove (ctx, ButtonRelease,
					     decor->release_cb_id);

  if (decor->xwin != None)
    {
      mb_wm_util_trap_x_errors();
      XDestroyWindow (decor->parent_client->wmref->xdpy, decor->xwin);
      mb_wm_util_untrap_x_errors();
    }

  memset (decor, 0, sizeof (*decor));
}

void
mb_wm_decor_set_theme_data (MBWMDecor * decor, void *userdata,
			    MBWMDecorDestroyUserData destroy)
{
  if (decor->themedata && decor->destroy_themedata)
    decor->destroy_themedata (decor, decor->themedata);

  decor->themedata = userdata;
  decor->destroy_themedata = destroy;
}

void *
mb_wm_decor_get_theme_data (MBWMDecor * decor)
{
  return decor->themedata;
}

/* Buttons */
static void
mb_wm_decor_button_destroy (MBWMObject* obj);

static void
mb_wm_decor_button_stock_button_action (MBWMDecorButton *button)
{
  MBWindowManagerClient *client = button->decor->parent_client;
  MBWindowManager       *wm = client->wmref;

  switch (button->type)
    {
    case MBWMDecorButtonClose:
      mb_wm_client_deliver_delete (client);
      break;
    case MBWMDecorButtonMinimize:
      mb_wm_client_iconize (client);
      break;
    case MBWMDecorButtonFullscreen:
      mb_wm_client_set_state (client,
			      MBWM_ATOM_NET_WM_STATE_FULLSCREEN,
			      MBWMClientWindowStateChangeAdd);
      break;
    case MBWMDecorButtonAccept:
      mb_wm_client_deliver_wm_protocol (client,
				wm->atoms[MBWM_ATOM_NET_WM_CONTEXT_ACCEPT]);
      break;
    case MBWMDecorButtonHelp:
      mb_wm_client_deliver_wm_protocol (client,
				wm->atoms[MBWM_ATOM_NET_WM_CONTEXT_HELP]);
      break;
    case MBWMDecorButtonMenu:
      mb_wm_client_deliver_wm_protocol (client,
				wm->atoms[MBWM_ATOM_NET_WM_CONTEXT_CUSTOM]);
    default:
      break;
    }

  return;
}

void
mb_wm_decor_button_set_theme_data (MBWMDecorButton * button, void *themedata,
			    MBWMDecorButtonDestroyUserData destroy)
{
  if (button->themedata && button->destroy_themedata)
    button->destroy_themedata (button, button->themedata);

  button->themedata = themedata;
  button->destroy_themedata = destroy;
}

void *
mb_wm_decor_button_get_theme_data (MBWMDecorButton * button)
{
  return button->themedata;
}

static Bool
mb_wm_decor_button_press_handler (XButtonEvent    *xev,
				  void            *userdata)
{
  MBWMDecorButton *button = (MBWMDecorButton *)userdata;
  MBWMDecor       *decor  = button->decor;
  MBWindowManager *wm;
  MBWMList        *transients = NULL;
  Bool             retval = True;

  if (!button->realized || !decor || !decor->parent_client)
    return False;

  wm = decor->parent_client->wmref;
  
  mb_wm_object_ref (MB_WM_OBJECT(button));

  if (xev->window == decor->xwin)
    {
      int xmin, ymin, xmax, ymax;
      MBWMList *l;

      transients = l = mb_wm_client_get_transients (decor->parent_client);

      /* Ignore events on the main window decor if transients other than
       * input methods are present
       */
      while (l)
	{
	  MBWindowManagerClient * c = l->data;

	  if (MB_WM_CLIENT_CLIENT_TYPE (c) != MBWMClientTypeInput &&
	      mb_wm_client_is_modal (c))
	    {
	      retval = True;
	      goto done;
	    }

	  l = l->next;
	}

      xmin = button->geom.x;
      ymin = button->geom.y;
      xmax = button->geom.x + button->geom.width;
      ymax = button->geom.y + button->geom.height;

      if (xev->x < xmin ||
	  xev->x > xmax ||
	  xev->y < ymin ||
	  xev->y > ymax)
	{
	  retval = True;
	  g_debug("%s not on button -- send GRAB_TRANSFER", __FUNCTION__);
	  XUngrabPointer(wm->xdpy, CurrentTime);
	  mb_wm_client_deliver_message (decor->parent_client,
					wm->atoms[MBWM_ATOM_MB_GRAB_TRANSFER],
					xev->time,
					xev->subwindow,
					xev->button, 0, 0);

	  XSync (wm->xdpy, False); /* Necessary */
	  goto done;
	}
      g_debug("%s on button", __FUNCTION__);

      if (button->state != MBWMDecorButtonStatePressed)
	{
	  button->state = MBWMDecorButtonStatePressed;
	  mb_wm_theme_paint_button (wm->theme, button);
	}

      if (button->press_activated)
	{
	  XUngrabPointer(wm->xdpy, CurrentTime);

	  mb_wm_client_deliver_message (decor->parent_client,
					wm->atoms[MBWM_ATOM_MB_GRAB_TRANSFER],
					xev->time,
					xev->subwindow,
					xev->button, 0, 0);

	  XSync (wm->xdpy, False); /* Necessary */

	  if (button->press)
	    button->press(wm, button, button->userdata);
	  else
	    mb_wm_decor_button_stock_button_action (button);
	}
      else
	{
	  XEvent ev;

	  /*
	   * First, call the custom function if any.
	   */
	  if (button->press)
	    button->press(wm, button, button->userdata);

	  if (XGrabPointer(wm->xdpy, xev->subwindow, False,
			   ButtonPressMask|ButtonReleaseMask|
			   PointerMotionMask|EnterWindowMask|LeaveWindowMask,
			   GrabModeAsync,
			   GrabModeAsync,
			   None, None, CurrentTime) == GrabSuccess)
	    {
              /* set up release handler to catch ButtonRelease while we
               * are spinning the main loop */
	      decor->release_cb_id = mb_wm_main_context_x_event_handler_add (
				 wm->main_ctx,
			         xev->subwindow,
			         ButtonRelease,
			         (MBWMXEventFunc)mb_wm_decor_release_handler,
			         decor);
	      if (button->state == MBWMDecorButtonStateInactive)
		{
		  button->state = MBWMDecorButtonStatePressed;
		  mb_wm_theme_paint_button (wm->theme, button);
		}

	      for (;;)
		{
		  /*
		   * First of all, we make sure that all events are flushed
		   * out (this is necessary to ensure that all the events we
		   * are interested in are actually intercepted here).
		   */
		  XSync (wm->xdpy, False);

		  /*
		   * Someone might destroy the window while we are waiting for
		   * the events here.
		   */
		  if (!button->realized)
                  {
                    /* if the window disappeared, ungrab was done by X.
                       Just remove the handler */
                    mb_wm_main_context_x_event_handler_remove (
                                   wm->main_ctx, ButtonRelease,
			           decor->release_cb_id);
                    decor->release_cb_id = 0;
		    mb_wm_object_unref (MB_WM_OBJECT(button));
		    return False;
		  }

                  if (!decor->release_cb_id)
                    {
                     /* the handler was called while we spinned the loop */
		     if (button->state == MBWMDecorButtonStatePressed)
		       {
                         button->state = MBWMDecorButtonStateInactive;
                         mb_wm_theme_paint_button (wm->theme, button);
                       }
		     return False;
                    }

		  if (XCheckMaskEvent(wm->xdpy,
				      ButtonPressMask|ButtonReleaseMask|
				      PointerMotionMask|EnterWindowMask|
				      LeaveWindowMask,
				      &ev))
		    {
		      switch (ev.type)
			{
			case MotionNotify:
			  {
			    XMotionEvent *pev = (XMotionEvent*)&ev;

			    if (pev->x < xmin || pev->x > xmax ||
				pev->y < ymin || pev->y > ymax)
			      {
				if (button->state ==
				    MBWMDecorButtonStatePressed)
				  {
				    button->state =
				      MBWMDecorButtonStateInactive;
				    mb_wm_theme_paint_button (wm->theme,button);
				  }
			      }
			    else
			      {
				if (button->state !=
				    MBWMDecorButtonStatePressed)
				  {
				    button->state = MBWMDecorButtonStatePressed;
				    mb_wm_theme_paint_button (wm->theme,button);
				  }
			      }
			  }
			  break;
			case EnterNotify:
			  if (button->state == MBWMDecorButtonStateInactive)
			    {
			      button->state = MBWMDecorButtonStatePressed;
			      mb_wm_theme_paint_button (wm->theme, button);
			    }
			  break;
			case LeaveNotify:
			  if (button->state != MBWMDecorButtonStateInactive)
			    {
			      button->state = MBWMDecorButtonStateInactive;
			      mb_wm_theme_paint_button (wm->theme, button);
			    }
			  break;
			case ButtonRelease:
			  {
			    XButtonEvent *pev = (XButtonEvent*)&ev;

			    if (button->state != MBWMDecorButtonStateInactive)
			      {
				button->state = MBWMDecorButtonStateInactive;
				mb_wm_theme_paint_button (wm->theme, button);
			      }

			    XUngrabPointer (wm->xdpy, CurrentTime);
			    XSync (wm->xdpy, False); /* necessary */

                            mb_wm_main_context_x_event_handler_remove (
                                   wm->main_ctx, ButtonRelease,
			           decor->release_cb_id);
                            decor->release_cb_id = 0;

			    if (pev->x < xmin || pev->x > xmax ||
				pev->y < ymin || pev->y > ymax)
			      {
				retval = False;
				goto done;
			      }

			    if (button->release)
			      button->release(wm, button, button->userdata);
			    else
			      mb_wm_decor_button_stock_button_action (button);

			    mb_wm_object_unref (MB_WM_OBJECT(button));
			    return False;
			  }
			}
		    }
		  else
		    {
		      /*
		       * No pending X event, so spin the main loop (this allows
		       * things like timers to work.
		       */
		      if (!mb_wm_main_context_spin_loop (wm->main_ctx))
                        /* no events, sleep a while so we don't busy loop */
                        g_usleep (1000 * 100);
		    }
		}
	    }
	}

      retval = False;
    }

 done:
  mb_wm_util_list_free (transients);
  mb_wm_object_unref (MB_WM_OBJECT(button));
  return retval;
}

static void
mb_wm_decor_button_class_init (MBWMObjectClass *klass)
{
#if MBWM_WANT_DEBUG
  klass->klass_name = "MBWMDecorButton";
#endif
}

static int
mb_wm_decor_button_init (MBWMObject *obj, va_list vap)
{
  MBWMDecorButton             *button = MB_WM_DECOR_BUTTON (obj);
  MBWindowManager             *wm = NULL;
  MBWMDecor                   *decor = NULL;
  MBWMDecorButtonPressedFunc   press = NULL;
  MBWMDecorButtonReleasedFunc  release = NULL;
  MBWMDecorButtonFlags         flags = 0;
  MBWMDecorButtonType          type = 0;
  MBWMDecorButtonPack          pack = MBWMDecorButtonPackEnd;
  MBWMObjectProp               prop;

  prop = va_arg(vap, MBWMObjectProp);
  while (prop)
    {
      switch (prop)
	{
	case MBWMObjectPropWm:
	  wm = va_arg(vap, MBWindowManager *);
	  break;
	case MBWMObjectPropDecor:
	  decor = va_arg(vap, MBWMDecor*);
	  break;
	case MBWMObjectPropDecorButtonPressedFunc:
	  press = va_arg(vap, MBWMDecorButtonPressedFunc);
	  break;
	case MBWMObjectPropDecorButtonReleasedFunc:
	  release = va_arg(vap, MBWMDecorButtonReleasedFunc);
	  break;
	case MBWMObjectPropDecorButtonFlags:
	  flags = va_arg(vap, MBWMDecorButtonFlags);
	  break;
	case MBWMObjectPropDecorButtonType:
	  type = va_arg(vap, MBWMDecorButtonType);
	  break;
	case MBWMObjectPropDecorButtonPack:
	  pack = va_arg(vap, MBWMDecorButtonPack);
	  break;
	default:
	  MBWMO_PROP_EAT (vap, prop);
	}

      prop = va_arg(vap, MBWMObjectProp);
    }

  if (!wm || !decor)
    return 0;

  /*
   * Decors must be attached before we can start adding buttons to them,
   * otherwise we cannot work out the button geometry.
   */
  MBWM_ASSERT (decor->parent_client);

  button->geom.width  = 0;
  button->geom.height = 0;

  mb_wm_theme_get_button_size (wm->theme,
			       decor,
			       type,
			       (int *)&button->geom.width,
			       (int *)&button->geom.height);

  button->press   = press;
  button->release = release;
  button->decor   = decor;
  button->type    = type;
  button->pack    = pack;
  button->flags   = flags;
  button->press_activated = mb_wm_theme_is_button_press_activated (wm->theme,
								   decor,
								   type);

  decor->buttons = mb_wm_util_list_append (decor->buttons, button);

  /* the decor assumes a reference, so add one for the caller */
  mb_wm_object_ref (obj);

  return 1;
}

int
mb_wm_decor_button_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (MBWMDecorButtonClass),
	sizeof (MBWMDecorButton),
	mb_wm_decor_button_init,
	mb_wm_decor_button_destroy,
	mb_wm_decor_button_class_init
      };

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_OBJECT, 0);
    }

  return type;
}

static void
mb_wm_decor_button_destroy (MBWMObject* obj)
{
  MBWMDecorButton * button = MB_WM_DECOR_BUTTON (obj);
  MBWMMainContext * ctx = NULL;
  
  ctx = button &&
	  button->decor &&
	  button->decor->parent_client &&
	  button->decor->parent_client->wmref ? 
	  button->decor->parent_client->wmref->main_ctx : NULL;

  if (!ctx)
	  return;
  /*
   * We are doing the job in the mb_wm_decor_button_unrealize() while the
   * decoration still exists.
   */
  mb_wm_main_context_x_event_handler_remove (ctx, ButtonPress,
					     button->press_cb_id);

  if (button->userdata && button->destroy_userdata)
    button->destroy_userdata (button, button->userdata);

  if (button->themedata && button->destroy_themedata)
    button->destroy_themedata (button, button->themedata);

  memset (button, 0, sizeof (*button));
}

static void
mb_wm_decor_button_realize (MBWMDecorButton *button)
{
  MBWMDecor           *decor = button->decor;
  MBWindowManager     *wm = decor->parent_client->wmref;

  if (!button->press_cb_id
      && !(button->flags & MB_WM_DECOR_BUTTON_NOHANDLERS))
  {
    button->press_cb_id =
      mb_wm_main_context_x_event_handler_add (wm->main_ctx,
			    decor->xwin,
			    ButtonPress,
			    (MBWMXEventFunc)mb_wm_decor_button_press_handler,
			    button);
  }

  button->realized = True;
}

static void
mb_wm_decor_button_unrealize (MBWMDecorButton *button)
{
  button->realized = False;
}

static void
mb_wm_decor_button_sync_window (MBWMDecorButton *button)
{
  if (!button->realized)
    {
      mb_wm_decor_button_realize (button);
    }
}

void
mb_wm_decor_button_show (MBWMDecorButton *button)
{
  button->visible = True;
}

void
mb_wm_decor_button_hide (MBWMDecorButton *button)
{
  button->visible = False;
}

void
mb_wm_decor_button_move_to (MBWMDecorButton *button, int x, int y)
{
  /* FIXME: set a sync flag so it know X movewindow is needed */
  button->geom.x = x;
  button->geom.y = y;

  MBWM_DBG ("#######  moving to %i, %i\n", button->geom.x, button->geom.y);
}

MBWMDecorButton*
mb_wm_decor_button_new (MBWindowManager              *wm,
			MBWMDecorButtonType           type,
			MBWMDecorButtonPack           pack,
			MBWMDecor                    *decor,
			MBWMDecorButtonPressedFunc    press,
			MBWMDecorButtonReleasedFunc   release,
			MBWMDecorButtonFlags          flags)
{
  MBWMObject  *button;

  button = mb_wm_object_new (MB_WM_TYPE_DECOR_BUTTON,
			     MBWMObjectPropWm,                      wm,
			     MBWMObjectPropDecorButtonType,         type,
			     MBWMObjectPropDecorButtonPack,         pack,
			     MBWMObjectPropDecor,                   decor,
			     MBWMObjectPropDecorButtonPressedFunc,  press,
			     MBWMObjectPropDecorButtonReleasedFunc, release,
			     MBWMObjectPropDecorButtonFlags,        flags,
			     NULL);

  return MB_WM_DECOR_BUTTON(button);
}

MBWMDecorButton*
mb_wm_decor_button_stock_new (MBWindowManager            *wm,
			      MBWMDecorButtonType         type,
			      MBWMDecorButtonPack         pack,
			      MBWMDecor                  *decor,
			      MBWMDecorButtonFlags        flags)
{
  MBWMObject  *button;

  button = mb_wm_object_new (MB_WM_TYPE_DECOR_BUTTON,
			     MBWMObjectPropWm,                      wm,
			     MBWMObjectPropDecorButtonType,         type,
			     MBWMObjectPropDecorButtonPack,         pack,
			     MBWMObjectPropDecor,                   decor,
			     MBWMObjectPropDecorButtonFlags,        flags,
			     NULL);

  return MB_WM_DECOR_BUTTON(button);
}

void
mb_wm_decor_button_handle_repaint (MBWMDecorButton *button)
{
  MBWMDecor *decor = button->decor;
  MBWMTheme *theme;

  if (decor->parent_client == NULL)
    return;

  theme = decor->parent_client->wmref->theme;
  mb_wm_theme_paint_button (theme, button);
}

void
mb_wm_decor_button_set_user_data (MBWMDecorButton * button, void *userdata,
				  MBWMDecorButtonDestroyUserData destroy)
{
  button->userdata = userdata;
  button->destroy_userdata = destroy;
}

void *
mb_wm_decor_button_get_user_data (MBWMDecorButton * button)
{
  return button->userdata;
}
