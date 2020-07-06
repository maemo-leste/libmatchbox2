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
#include "mb-wm-client-dialog.h"

#include <unistd.h>
#include <signal.h>

#if ENABLE_COMPOSITE
#include <X11/extensions/Xrender.h>
#endif

/**
 * Private data for MBWindowManagerClient.
 */
struct MBWindowManagerClientPriv
{
  Bool          realized;
  Bool          mapped;
  Bool          map_confirmed;
  Bool          unmap_confirmed;
  Bool          iconizing;
  Bool          hiding_from_desktop;
  Bool          geometry_requested;
  MBWMSyncType  sync_state;
};

static void
mb_wm_client_destroy (MBWMObject *obj)
{
  MBWindowManagerClient * client = MB_WM_CLIENT(obj);
  MBWindowManager       * wm = client->wmref;
  MBWMList              * l;

  if (client->sig_theme_change_id)
    mb_wm_object_signal_disconnect (MB_WM_OBJECT (wm),
				    client->sig_theme_change_id);

  if (client->sig_prop_change_id)
    mb_wm_object_signal_disconnect (MB_WM_OBJECT (client->window),
				    client->sig_prop_change_id);

  if (client->ping_cb_id)
    mb_wm_main_context_timeout_handler_remove (wm->main_ctx,
					       client->ping_cb_id);

#if ENABLE_COMPOSITE
  if (mb_wm_compositing_enabled (wm))
    {
      mb_wm_comp_mgr_unregister_client (wm->comp_mgr, client);
    }
#endif

  mb_wm_object_unref (MB_WM_OBJECT (client->window));

  for (l = client->decor; l; l = l->next)
    mb_wm_object_unref (l->data);

  if (client->decor)
    mb_wm_util_list_free (client->decor);

  if (client->transient_for)
    mb_wm_client_remove_transient (client->transient_for, client);

  if (client->priv)
    free (client->priv);

  memset (client, 0, sizeof (*client));
}

static Bool
mb_wm_client_on_property_change (MBWMClientWindow *window,
				 int               property,
				 void             *userdata);

static Bool
mb_wm_client_on_theme_change (MBWindowManager * wm, int signal,
			      MBWindowManagerClient * client)
{
  mb_wm_client_theme_change (client);
  return False;
}


static int
mb_wm_client_init (MBWMObject *obj, va_list vap)
{
  MBWindowManagerClient *client;
  MBWindowManager       *wm = NULL;
  MBWMClientWindow      *win = NULL;
  MBWMObjectProp         prop;

  prop = va_arg(vap, MBWMObjectProp);
  while (prop)
    {
      switch (prop)
	{
	case MBWMObjectPropWm:
	  wm = va_arg(vap, MBWindowManager *);
	  break;
	case MBWMObjectPropClientWindow:
	  win = va_arg(vap, MBWMClientWindow *);
	  break;
	default:
	  MBWMO_PROP_EAT (vap, prop);
	}

      prop = va_arg(vap, MBWMObjectProp);
    }

  if (!wm)
    return 0;

  MBWM_MARK();

  client = MB_WM_CLIENT(obj);
  client->priv   = mb_wm_util_malloc0(sizeof(MBWindowManagerClientPriv));

  client->window        = win;
  client->wmref         = wm;
  client->ping_timeout  = 6000;

  if (wm->theme)
    {
      client->layout_hints =
	mb_wm_theme_get_client_layout_hints (wm->theme, client);
    }

  /* sync with client window */
  mb_wm_client_on_property_change (win, MBWM_WINDOW_PROP_ALL, client);

  /* Handle underlying property changes */
  client->sig_prop_change_id =
    mb_wm_object_signal_connect (MB_WM_OBJECT (win),
		 MBWM_WINDOW_PROP_ALL,
		 (MBWMObjectCallbackFunc)mb_wm_client_on_property_change,
		 client);

  client->sig_theme_change_id =
    mb_wm_object_signal_connect (MB_WM_OBJECT (wm),
		 MBWindowManagerSignalThemeChange,
		 (MBWMObjectCallbackFunc)mb_wm_client_on_theme_change,
		 client);

#if ENABLE_COMPOSITE
  {
    XRenderPictFormat *format;

    format = XRenderFindVisualFormat (wm->xdpy, win->visual);

    if (format && format->type == PictTypeDirect &&
	format->direct.alphaMask)
      client->is_argb32 = True;
  }
#endif

  return 1;
}

int
mb_wm_client_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (MBWindowManagerClientClass),
	sizeof (MBWindowManagerClient),
	mb_wm_client_init,
	mb_wm_client_destroy,
	NULL
      };

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_OBJECT, 0);
    }

  return type;
}

void
mb_wm_client_fullscreen_mark_dirty (MBWindowManagerClient *client)
{
  /* fullscreen implies geometry and visibility sync */
  mb_wm_display_sync_queue (client->wmref,
			    MBWMSyncFullscreen |
			    MBWMSyncVisibility | MBWMSyncGeometry);

  client->priv->sync_state |= (MBWMSyncFullscreen |
			       MBWMSyncGeometry   |
			       MBWMSyncVisibility);
}

void
mb_wm_client_stacking_mark_dirty (MBWindowManagerClient *client)
{
  mb_wm_display_sync_queue (client->wmref, MBWMSyncStacking);
  client->priv->sync_state |= MBWMSyncStacking;
}

void
mb_wm_client_geometry_mark_dirty (MBWindowManagerClient *client)
{
  mb_wm_display_sync_queue (client->wmref, MBWMSyncGeometry);

  client->priv->sync_state |= MBWMSyncGeometry;
}

void
mb_wm_client_visibility_mark_dirty (MBWindowManagerClient *client)
{
  mb_wm_display_sync_queue (client->wmref, MBWMSyncVisibility);

  client->priv->sync_state |= MBWMSyncVisibility;

  MBWM_DBG(" sync state: %i", client->priv->sync_state);
}

void
mb_wm_client_configure_request_ack_queue (MBWindowManagerClient *client)
{
  mb_wm_display_sync_queue (client->wmref, MBWMSyncConfigRequestAck);

  client->priv->sync_state |= MBWMSyncConfigRequestAck;

  MBWM_DBG(" sync state: %i", client->priv->sync_state);
}

Bool
mb_wm_client_needs_configure_request_ack (MBWindowManagerClient *client)
{
  return (client->priv->sync_state & MBWMSyncConfigRequestAck);
}

void
mb_wm_client_decor_mark_dirty (MBWindowManagerClient *client)
{
  mb_wm_display_sync_queue (client->wmref, MBWMSyncDecor);

  client->priv->sync_state |= MBWMSyncDecor;

  MBWM_DBG(" sync state: %i", client->priv->sync_state);
}

Bool
mb_wm_client_needs_fullscreen_sync (MBWindowManagerClient *client)
{
  return (client->priv->sync_state & MBWMSyncFullscreen);
}

static Bool
mb_wm_client_on_property_change (MBWMClientWindow        *window,
				 int                      property,
				 void                    *userdata)
{
  MBWindowManagerClient * client = MB_WM_CLIENT (userdata);

  if (property & MBWM_WINDOW_PROP_NAME)
    {
      MBWMList * l = client->decor;
      while (l)
	{
	  MBWMDecor * decor = l->data;

	  if (mb_wm_decor_get_type (decor) == MBWMDecorTypeNorth)
	    {
	      mb_wm_decor_mark_title_dirty (decor);
	      break;
	    }

	  l = l->next;
	}
    }

  if (property & MBWM_WINDOW_PROP_GEOMETRY)
    mb_wm_client_geometry_mark_dirty (client);

  if ((property & MBWM_WINDOW_PROP_HILDON_STACKING) && client->window)
    {
      if (client->window->hildon_stacking_layer > 0)
        client->stacking_layer = client->window->hildon_stacking_layer
		                 + MBWMStackLayerHildon1 - 1;
      mb_wm_client_stacking_mark_dirty (client);
    }

#if ENABLE_COMPOSITE
  if ((property & MBWM_WINDOW_PROP_CM_TRANSLUCENCY) &&
      client->cm_client && mb_wm_comp_mgr_enabled (client->wmref->comp_mgr))
    {
      mb_wm_comp_mgr_client_repair (client->cm_client, None);
    }
#endif

  if (property & MBWM_WINDOW_PROP_PORTRAIT)
    {
      client->portrait_supported            = window->portrait_supported > 0;
      client->portrait_supported_inherited  = window->portrait_supported < 0;
      client->portrait_requested            = window->portrait_requested > 0
                                              ? window->portrait_requested : 0;
      client->portrait_requested_inherited  = window->portrait_requested < 0;
      g_debug ("portrait properties of %p: supported=%d requested=%d", client,
               client->portrait_supported_inherited
                 ? -1 : client->portrait_supported,
               client->portrait_requested_inherited
                 ? -1 : client->portrait_requested);
    }

  return False;
}

MBWindowManagerClient* 	/* FIXME: rename to mb_wm_client_base/class_new ? */
mb_wm_client_new (MBWindowManager *wm, MBWMClientWindow *win)
{
  MBWindowManagerClient *client = NULL;

  client = MB_WM_CLIENT(mb_wm_object_new (MB_WM_TYPE_CLIENT,
					  MBWMObjectPropWm,           wm,
					  MBWMObjectPropClientWindow, win,
					  NULL));

  return client;
}

void
mb_wm_client_realize (MBWindowManagerClient *client)
{
  MBWindowManagerClientClass *klass;

  if (client->priv->realized)
    return;

  klass = MB_WM_CLIENT_CLASS (MB_WM_OBJECT_GET_CLASS (client));

  if (klass->realize)
    klass->realize(client);

  client->priv->realized = True;
}

Bool
mb_wm_client_is_realized (MBWindowManagerClient *client)
{
  return client->priv->realized;
}


/* ## Stacking ## */


void
mb_wm_client_stack (MBWindowManagerClient *client,
		    int                    flags)
{
  MBWindowManagerClientClass *klass;

  if (client->window->live_background)
    {
      /* stack live background to the bottom */
      mb_wm_stack_remove (client);
      mb_wm_stack_prepend_bottom (client);
      return;
    }

  klass = MB_WM_CLIENT_CLASS (MB_WM_OBJECT_GET_CLASS (client));

  if (klass->stack)
    {
      klass->stack(client, flags);

      /* Schedule stack sync, but not if the client is of override type */
      if (MB_WM_CLIENT_CLIENT_TYPE (client) != MBWMClientTypeOverride)
	mb_wm_client_stacking_mark_dirty (client);
    }
}

void
mb_wm_client_show (MBWindowManagerClient *client)
{
  MBWindowManagerClientClass *klass;

  klass = MB_WM_CLIENT_CLASS (MB_WM_OBJECT_GET_CLASS (client));

  if (klass->show)
    klass->show (client);

  client->priv->mapped = True;

  /* Make sure any Hidden state flag is cleared */
  mb_wm_client_set_state (client,
			  MBWM_ATOM_NET_WM_STATE_HIDDEN,
			  MBWMClientWindowStateChangeRemove);

  mb_wm_client_visibility_mark_dirty (client);
}

void
mb_wm_client_hide (MBWindowManagerClient *client)
{
  MBWindowManagerClientClass *klass;

  klass = MB_WM_CLIENT_CLASS (MB_WM_OBJECT_GET_CLASS (client));

  if (klass->hide)
    klass->hide (client);

  client->priv->mapped = False;

  mb_wm_unfocus_client (client->wmref, client);
  mb_wm_client_visibility_mark_dirty (client);
}

/*
 * The focus processing is split into two stages, client and WM
 *
 * The client stage is handled by this function, with the return value
 * of True indicating that the focus has changed.
 *
 * NB: This function should be considered protected and only called by the
 * MBWindowManager object code.
 */
Bool
mb_wm_client_focus (MBWindowManagerClient *client)
{
  MBWindowManagerClientClass *klass;
  Bool ret = False;

  klass = MB_WM_CLIENT_CLASS (MB_WM_OBJECT_GET_CLASS (client));

  if (klass->focus)
    ret = klass->focus(client);

  return ret;
}

Bool
mb_wm_client_want_focus (MBWindowManagerClient *client)
{
  return (client->window->want_key_input != False);
}

Bool
mb_wm_client_needs_visibility_sync (MBWindowManagerClient *client)
{
  return (client->priv->sync_state & MBWMSyncVisibility);
}

Bool
mb_wm_client_needs_geometry_sync (MBWindowManagerClient *client)
{
  return (client->priv->sync_state & MBWMSyncGeometry);
}

Bool
mb_wm_client_needs_decor_sync (MBWindowManagerClient *client)
{
  return (client->priv->sync_state & MBWMSyncDecor);
}

Bool
mb_wm_client_needs_sync (MBWindowManagerClient *client)
{
  MBWM_DBG("sync_state: %i", client->priv->sync_state);

  return client->priv->sync_state;
}

Bool
mb_wm_client_is_mapped (MBWindowManagerClient *client)
{
  return client->priv->mapped;
}

Bool
mb_wm_client_is_map_confirmed (MBWindowManagerClient *client)
{
  return client->priv->map_confirmed;
}

void
mb_wm_client_set_map_confirmed (MBWindowManagerClient *client,
                                Bool confirmed)
{
  client->priv->map_confirmed = confirmed;
}

Bool
mb_wm_client_is_unmap_confirmed (MBWindowManagerClient *client)
{
  return client->priv->unmap_confirmed;
}

Bool
mb_wm_client_is_geometry_requested (MBWindowManagerClient *client)
{
  return client->priv->geometry_requested;
}

void
mb_wm_client_set_unmap_confirmed (MBWindowManagerClient *client,
                                  Bool confirmed)
{
  client->priv->unmap_confirmed = confirmed;
}

void
mb_wm_client_display_sync (MBWindowManagerClient *client)
{
  MBWindowManagerClientClass *klass;

  klass = MB_WM_CLIENT_CLASS (MB_WM_OBJECT_GET_CLASS (client));

  if (klass->sync)
    klass->sync (client);

  client->priv->sync_state = 0;
}


Bool
mb_wm_client_request_geometry (MBWindowManagerClient *client,
			       MBGeometry            *new_geometry,
			       MBWMClientReqGeomType  flags)
{
  MBWindowManagerClientClass *klass;
  Bool ret = False;

  klass = MB_WM_CLIENT_CLASS (MB_WM_OBJECT_GET_CLASS (client));

  if (klass->geometry)
    ret = klass->geometry(client, new_geometry, flags);

  client->priv->geometry_requested = TRUE;

  return ret;
}

void
mb_wm_client_set_layout_hints (MBWindowManagerClient *client,
			       MBWMClientLayoutHints  hints)
{
  client->layout_hints = hints;
}

void
mb_wm_client_set_layout_hint (MBWindowManagerClient *client,
			      MBWMClientLayoutHints  hint,
			      Bool                   state)
{
  if (state)
    client->layout_hints |= hint;
  else
    client->layout_hints &= ~hint;
}

void  /* needs to be boolean, client may not have any coverage */
mb_wm_client_get_coverage (MBWindowManagerClient *client,
			   MBGeometry            *coverage)
{
  MBGeometry *geometry;

  if (!client->xwin_frame)
    geometry = &client->window->geometry;
  else
    geometry = &client->frame_geometry;

  coverage->x      = geometry->x;
  coverage->y      = geometry->y;
  coverage->width  = geometry->width;
  coverage->height = geometry->height;
}

void
mb_wm_client_add_transient (MBWindowManagerClient *client,
			    MBWindowManagerClient *transient)
{
  MBWMList *l;

  if (transient == NULL || client == NULL)
    return;

  /*
   * If this transient already has a registered transient parent we need to
   * remove the link from the parent.
   */
  if (transient->transient_for)
    mb_wm_client_remove_transient (transient->transient_for, transient);

  transient->transient_for = client;

  /*
   * Make sure that each transient is only added once (theoretically it should
   * be, but it is very easy for a derived class to call this function without
   * realizing the parent has dones so).
   */
  l = client->transients;
  while (l)
    {
      if (l->data == transient)
	return;

      l = l->next;
    }

  client->transients = mb_wm_util_list_append(client->transients, transient);
}

void
mb_wm_client_remove_transient (MBWindowManagerClient *client,
			       MBWindowManagerClient *transient)
{
  if (!transient || !client || !client->transients)
    return;

  transient->transient_for = NULL;

  client->transients = mb_wm_util_list_remove(client->transients, transient);
}

void
mb_wm_client_remove_all_transients (MBWindowManagerClient *client)
{
  MBWMList *cursor = client->transients;

  while (cursor)
    {
      MBWindowManagerClient *transient = cursor->data;
      transient->transient_for = NULL;
      transient->window->xwin_transient_for = None;
      cursor = cursor->next;
    }

  mb_wm_util_list_free (client->transients);
  client->transients = NULL;
}

/**
 * Returns the client next above the given client in the
 * stacking order.  Returns NULL if the client does not
 * exist or is the topmost client.
 */
MBWindowManagerClient*
mb_wm_client_get_next_focused_client (MBWindowManagerClient *client)
{
  return client->stacked_above;
}

/**
 * Returns the app next above the given client in the
 * stacking order.  Returns NULL if there is no app
 * higher than this client.
 */
MBWindowManagerClient*
mb_wm_client_get_next_focused_app (MBWindowManagerClient *client)
{
  while (client)
    {
      client = client->stacked_above;
      if (client && MB_WM_CLIENT_CLIENT_TYPE (client)==MBWMClientTypeApp)
	return client;
    }
  return NULL;
}

/**
 * Returns the transient of client "client" which was most recently
 * focused (i.e. which is closest to the top, in our model).
 * Returns NULL if there are no transients for the given client.
 */
MBWindowManagerClient*
mb_wm_client_get_last_focused_transient (MBWindowManagerClient *client)
{
  MBWindowManagerClient *result = NULL, *c;
  MBWindowManager       *wm = client->wmref;

  mb_wm_stack_enumerate_reverse (wm, c)
    if (c->transient_for == client &&
        c->window->net_type !=
	  wm->atoms[MBWM_ATOM_HILDON_WM_WINDOW_TYPE_ANIMATION_ACTOR])
      {
        result = c;
        break;
      }

  if (result)
    {
      MBWindowManagerClient  *rec_transient;

      rec_transient = mb_wm_client_get_last_focused_transient (result);
      if (rec_transient)
        result = rec_transient;
    }

  return result;
}

MBWMList*
mb_wm_client_get_transients (MBWindowManagerClient *client)
{
  MBWMList *trans = NULL;
  MBWMList *l = client->transients;
  Window    xgroup = client->window->xwin_group;
  MBWindowManagerClient *c;
  MBWMClientType c_type = MB_WM_CLIENT_CLIENT_TYPE (client);

  while (l)
    {
      trans = mb_wm_util_list_append (trans, l->data);
      l = l->next;
    }

  /* If this is an application or desktop that are part of an group,
   * we add any other transients that are part of the group to the list.
   */
  if (xgroup &&
      (c_type == MBWMClientTypeApp || c_type == MBWMClientTypeDesktop))
    {
      mb_wm_stack_enumerate (client->wmref, c)
	if (c != client &&
	    c->transient_for && c->window->xwin_group == xgroup)
	{
	  MBWindowManagerClient * t = c->transient_for;

	  /* Only add it if it is not directly transiet for our client (in
	   * which case it is already in the list
	   *
	   * Find the bottom level transient
	   */
	  while (t && t->transient_for)
	    t = t->transient_for;

	  if (!t || (MB_WM_CLIENT_CLIENT_TYPE (t) == MBWMClientTypeApp ||
		     MB_WM_CLIENT_CLIENT_TYPE (t) == MBWMClientTypeDesktop))
	    {
	      trans = mb_wm_util_list_append (trans, c);
	    }
	}
    }

  return trans;
}


/*
 * Returns TRUE iff the named client is a system-modal dialogue.
 *
 * Note: We now say that a system-modal dialogue is any dialogue which
 * is intransient, or is transient to itself or to the root; a system-
 * modal dialogue is not required to be modal.  In other words, testing
 * "!mb_wm_client_get_transient_for(w)" is equivalent to testing whether
 * w is system-modal.
 */
gboolean
mb_wm_client_is_system_modal (MBWindowManagerClient *client)
{
  if (!client)
    return FALSE;

  if (!mb_wm_object_is_descendant ((MBWMObject*)client,
				   MB_WM_TYPE_CLIENT_DIALOG))
    return FALSE;

  return
    /* It can be transient to nothing... */
    client->window->xwin_transient_for == None ||
    /* ...or to itself... */
    client->window->xwin_transient_for == client->window->xwindow ||
    /* ...or to the root. */
    client->window->xwin_transient_for ==
    client->wmref->root_win->xwindow;
}

const char*
mb_wm_client_get_name (MBWindowManagerClient *client)
{
  if (!client->window)
    return NULL;

  return client->window->name;
}

gboolean
mb_wm_client_deliver_message (MBWindowManagerClient   *client,
			      Atom          delivery_atom,
			      unsigned long data0,
			      unsigned long data1,
			      unsigned long data2,
			      unsigned long data3,
			      unsigned long data4)
{
  MBWindowManager *wm = client->wmref;
  Window xwin = client->window->xwindow;
  XEvent ev;
  int status;

  memset(&ev, 0, sizeof(ev));

  ev.xclient.type = ClientMessage;
  ev.xclient.window = xwin;
  ev.xclient.message_type = delivery_atom;
  ev.xclient.format = 32;
  ev.xclient.data.l[0] = data0;
  ev.xclient.data.l[1] = data1;
  ev.xclient.data.l[2] = data2;
  ev.xclient.data.l[3] = data3;
  ev.xclient.data.l[4] = data4;

  /* We can do this asynchronously because whenever this function is
   * called when a sync is required, XSync is called after it. */
  mb_wm_util_async_trap_x_errors_warn(wm->xdpy, "XSendEvent");
  status = XSendEvent(wm->xdpy, xwin, False, NoEventMask, &ev);
  mb_wm_util_async_untrap_x_errors();
  return status!=0;
}

void
mb_wm_client_deliver_wm_protocol (MBWindowManagerClient *client,
				  Atom protocol)
{
  MBWindowManager *wm = client->wmref;

  mb_wm_client_deliver_message (client, wm->atoms[MBWM_ATOM_WM_PROTOCOLS],
				protocol, CurrentTime, 0, 0, 0);
}

static void
mb_wm_client_deliver_ping_protocol (MBWindowManagerClient *client)
{
  MBWindowManager *wm = client->wmref;

  mb_wm_client_deliver_message (client,
				wm->atoms[MBWM_ATOM_WM_PROTOCOLS],
				wm->atoms[MBWM_ATOM_NET_WM_PING],
				CurrentTime,
				client->window->xwindow,
				0, 0);
}

static Bool
mb_wm_client_ping_timeout_cb (void *userdata)
{
  MBWindowManagerClient *client = userdata;
  MBWindowManager *wm = client->wmref;

  g_debug ("%s: entered", __FUNCTION__);
  mb_wm_handle_hang_client (wm, client);
  return False;
}

void
mb_wm_client_ping_start (MBWindowManagerClient *client)
{
  MBWindowManager * wm = client->wmref;
  MBWMMainContext * ctx = wm->main_ctx;

  if (client->ping_cb_id)
    return;

  client->ping_cb_id =
    mb_wm_main_context_timeout_handler_add (ctx, client->ping_timeout,
					    mb_wm_client_ping_timeout_cb,
					    client);

  mb_wm_client_deliver_ping_protocol (client);
}

void
mb_wm_client_ping_stop (MBWindowManagerClient *client)
{
  MBWMMainContext * ctx = client->wmref->main_ctx;

  g_debug ("%s: entered", __FUNCTION__);
  if (!client->ping_cb_id)
    return;

  mb_wm_main_context_timeout_handler_remove (ctx, client->ping_cb_id);
  client->ping_cb_id = 0;
}

void
mb_wm_client_shutdown (MBWindowManagerClient *client)
{
  char               buf[257];
  Window             xwin;
  const char        *machine;
  pid_t              pid;

  if (!client || !client->window)
    return;

  xwin = client->window->xwindow;
  machine = client->window->machine;
  pid = client->window->pid;

  if (machine && pid && (gethostname (buf, sizeof(buf)-1) == 0))
    {
      if (!strcmp (buf, machine))
	{
	  if (pid==getpid())
	    {
	      /* attempting to nuke ourselves; see NB#122710. */
	      g_warning ("Window manager may not kill itself\n");
	      return;
	    }

          g_debug ("%s: kill(%u, SIGKILL)", __FUNCTION__, pid);
	  if (kill (pid, SIGKILL) >= 0)
	    return;
	}
    }

  g_debug ("%s: XKillClient for %lx", __FUNCTION__, xwin);
  XKillClient(client->wmref->xdpy, xwin);
}

void
mb_wm_client_deliver_delete (MBWindowManagerClient *client)
{
  MBWindowManager        *wm     = client->wmref;
  MBWMClientWindowProtos  protos = client->window->protos;

  if ((protos & MBWMClientWindowProtosDelete))
    {
      mb_wm_client_deliver_wm_protocol (client,
				wm->atoms[MBWM_ATOM_WM_DELETE_WINDOW]);

      mb_wm_client_ping_start (client);
    }
  else
    mb_wm_client_shutdown (client);
}

void
mb_wm_client_set_state (MBWindowManagerClient *client,
			MBWMAtom state,
			MBWMClientWindowStateChange state_op)
{
  MBWindowManager   *wm  = client->wmref;
  MBWMClientWindow  *win = client->window;
  Bool               old_state;
  Bool               new_state = False;
  MBWMClientWindowEWMHState state_flag;

  switch (state)
    {
    case MBWM_ATOM_NET_WM_STATE_FULLSCREEN:
      state_flag = MBWMClientWindowEWMHStateFullscreen;
      break;
    case MBWM_ATOM_NET_WM_STATE_ABOVE:
      state_flag = MBWMClientWindowEWMHStateAbove;
      break;
    case MBWM_ATOM_NET_WM_STATE_HIDDEN:
      state_flag = MBWMClientWindowEWMHStateHidden;
      break;
    case MBWM_ATOM_NET_WM_STATE_SKIP_TASKBAR:
      state_flag = MBWMClientWindowEWMHStateSkipTaskbar;
      break;
    case MBWM_ATOM_NET_WM_STATE_MODAL:
    case MBWM_ATOM_NET_WM_STATE_STICKY:
    case MBWM_ATOM_NET_WM_STATE_MAXIMIZED_VERT:
    case MBWM_ATOM_NET_WM_STATE_MAXIMIZED_HORZ:
    case MBWM_ATOM_NET_WM_STATE_SHADED:
    case MBWM_ATOM_NET_WM_STATE_SKIP_PAGER:
    case MBWM_ATOM_NET_WM_STATE_BELOW:
    case MBWM_ATOM_NET_WM_STATE_DEMANDS_ATTENTION:
    default:
      return; /* not handled yet */
    }

  old_state = (win->ewmh_state & state_flag) ? True : False;

  switch (state_op)
    {
    case MBWMClientWindowStateChangeRemove:
      new_state = False;
      break;
    case MBWMClientWindowStateChangeAdd:
      new_state = True;
      break;
    case MBWMClientWindowStateChangeToggle:
      new_state = !old_state;
      break;
    }

  if (new_state == old_state)
    return;

  if (new_state)
    {
      win->ewmh_state |= state_flag;
    }
  else
    {
      win->ewmh_state &= ~state_flag;
    }

  if (state_flag & MBWMClientWindowEWMHStateHidden)
    {
      if (new_state)
	mb_wm_client_hide (client);
      else
	mb_wm_client_show (client);

      return;
    }

  if ((state_flag & MBWMClientWindowEWMHStateFullscreen))
    {
      mb_wm_client_fullscreen_mark_dirty (client);
      mb_wm_display_sync_queue (wm, MBWMSyncStacking);
    }

  /*
   * FIXME -- resize && move, possibly redraw decors when returning from
   * fullscreen
   */
}

Bool
mb_wm_client_ping_in_progress (MBWindowManagerClient * client)
{
  return (client->ping_cb_id != 0);
}

void
mb_wm_client_theme_change (MBWindowManagerClient *client)
{
  MBWindowManagerClientClass *klass;

  klass = MB_WM_CLIENT_CLASS (MB_WM_OBJECT_GET_CLASS (client));

  if (klass->theme_change)
    klass->theme_change (client);
}

/*
 * Cleanup any transient relationships this client might have
 * (we need to do this when the client window unmaps to ensure correct
 * functioning of the stack).
 */
void
mb_wm_client_detransitise (MBWindowManagerClient *client)
{
  MBWindowManagerClientClass *klass;

  if (!client->transient_for)
    return;

  klass = MB_WM_CLIENT_CLASS (MB_WM_OBJECT_GET_CLASS (client));

  if (klass->detransitise)
    klass->detransitise (client);

  mb_wm_client_remove_transient (client->transient_for, client);
}

Bool
mb_wm_client_is_iconizing (MBWindowManagerClient *client)
{
  return client->priv->iconizing;
}

void
mb_wm_client_reset_iconizing (MBWindowManagerClient *client)
{
  client->priv->iconizing = False;
}

void
mb_wm_client_iconize (MBWindowManagerClient *client)
{
  XUnmapWindow(client->wmref->xdpy, client->window->xwindow);
}

int
mb_wm_client_title_height (MBWindowManagerClient *client)
{
  MBWindowManager  * wm  = client->wmref;
  int                north;

  mb_wm_theme_get_decor_dimensions (wm->theme, client,
				    &north, NULL, NULL, NULL);

  return north;
}

Bool
mb_wm_client_is_modal (MBWindowManagerClient *client)
{
  return mb_wm_client_window_is_state_set (client->window,
					   MBWMClientWindowEWMHStateModal);
}

Bool
mb_wm_client_owns_xwindow (MBWindowManagerClient *client, Window xwin)
{
  MBWMList * l;

  if (client->xwin_frame == xwin || client->window->xwindow == xwin)
    return True;

  l = client->decor;
  while (l)
    {
      MBWMDecor * d = l->data;

      if (d->xwin == xwin)
	return True;

      l = l->next;
    }

  return False;
}

MBWMStackLayerType
mb_wm_client_get_stacking_layer (MBWindowManagerClient *client)
{
  MBWindowManagerClientClass *klass;

  if (client->window->live_background)
    /* stack live background to the bottom */
    return MBWMStackLayerUnknown;

  klass = MB_WM_CLIENT_CLASS (MB_WM_OBJECT_GET_CLASS (client));

  if (klass->stacking_layer)
    return klass->stacking_layer (client);

  return client->stacking_layer;
}

Bool
mb_wm_client_is_argb32 (MBWindowManagerClient *client)
{
  return client->is_argb32;
}

void
mb_wm_client_set_desktop (MBWindowManagerClient * client, int desktop)
{
  client->desktop = desktop;
}

int
mb_wm_client_get_desktop (MBWindowManagerClient * client)
{
  return client->desktop;
}

void
mb_wm_client_desktop_change (MBWindowManagerClient * client, int desktop)
{
  if (client->desktop < 0)
    return;

  if (client->desktop == desktop)
    {
      mb_wm_client_set_state (client,
			      MBWM_ATOM_NET_WM_STATE_HIDDEN,
			      MBWMClientWindowStateChangeRemove);

      /*
       * NB -- we do not reset the hiding_from_desktop flag here
       * since it is there to indicate to the WM that the window is
       * mapping because of the desktop change; the WM resets it when
       * it get the map-notify for it.
       */
    }
  else
    {
      client->priv->hiding_from_desktop = True;

      mb_wm_client_set_state (client,
			      MBWM_ATOM_NET_WM_STATE_HIDDEN,
			      MBWMClientWindowStateChangeAdd);
    }
}

Bool
mb_wm_client_is_hiding_from_desktop (MBWindowManagerClient * client)
{
  return client->priv->hiding_from_desktop;
}

void
mb_wm_client_reset_hiding_from_desktop (MBWindowManagerClient * client)
{
  client->priv->hiding_from_desktop = False;
}

/**
 * Returns true iff the client is visible: it must be
 * mapped, not hiding from the desktop, and at least
 * partially onscreen.
 * Does not check whether it's obscured by a higher
 * window (should it?)
 */
Bool
mb_wm_client_is_visible (MBWindowManagerClient * client)
{
  MBGeometry geometry;
  MBWindowManager *wm = client->wmref;

  mb_wm_client_get_coverage (client, &geometry);

  return
    mb_wm_client_is_mapped (client) &&
    !mb_wm_client_is_hiding_from_desktop (client) &&
    geometry.x < wm->xdpy_width &&
    geometry.y < wm->xdpy_height &&
    geometry.x+geometry.width >= 0 &&
    geometry.y+geometry.height >= 0;
}

/**
 * Returns true iff the client (including its frame)
 * covers at least the whole screen: it must be either
 * maximised or fullscreen.
 */
Bool
mb_wm_client_covers_screen (MBWindowManagerClient * client)
{
  MBGeometry geometry;
  MBWindowManager *wm = client->wmref;
  int right, bottom,
    right_of_screen = wm->xdpy_width,
    bottom_of_screen = wm->xdpy_height;

  mb_wm_client_get_coverage (client, &geometry);
  right = geometry.x+geometry.width;
  bottom = geometry.y+geometry.height;

  return
    geometry.x <= 0 &&
    geometry.y <= 0 &&
    right >= right_of_screen &&
    bottom >= bottom_of_screen;
}

/* Returns whether we're confident the newly mapped @client wants
 * the screen to be rotated. */
Bool
mb_wm_client_wants_portrait (MBWindowManagerClient * client)
{
  if (client->window->portrait_requested <= 0)
    /* Out of scope. */
    return False;

  if (!(MB_WM_CLIENT_CLIENT_TYPE (client)
        & (MBWMClientTypeApp|MBWMClientTypeDialog)))
    /* Only care about applications (full screen coverage) and dialogs. */
    return False;

  if ((MB_WM_CLIENT_CLIENT_TYPE (client) & MBWMClientTypeDialog)
      && client->window->portrait_requested < 2)
    /* But only dialogs which demand rotation. */
    return False;

  /* If we cannot say for sure hd will decide. */
  return True;
}

/* Update the inherited portrait flags of @cs if they were calculated
 * earlier than @now.  If @now is G_MAXUINT the flags are uncoditionally
 * updated but are not cached. */
void
mb_wm_client_update_portrait_flags (MBWindowManagerClient *cs, guint now)
{
  MBWindowManagerClient *ct;

  if ((cs->portrait_supported_inherited
       || cs->portrait_requested_inherited)
      && cs->portrait_timestamp != now)
    { /* @cs has outdated flags */
      if (  !cs->portrait_requested_inherited
          && cs->portrait_requested
          && cs->portrait_supported_inherited)
        /* Add some crap to the pile: if you request but don't say
         * you support you do. */
        cs->portrait_supported = TRUE;
      else if ((MB_WM_CLIENT_CLIENT_TYPE (cs) & MBWMClientTypeDialog)
               && cs->portrait_supported_inherited)
          /* Window dialog should inherit flags from the parent. */
        cs->portrait_supported = TRUE;
      else if (cs->transient_for)
        { /* Get the parent's and copy them. */
          mb_wm_client_update_portrait_flags (ct = cs->transient_for, now);
          if (cs->portrait_supported_inherited)
            cs->portrait_supported = ct->portrait_supported;
          if (cs->portrait_requested_inherited)
            cs->portrait_requested = ct->portrait_requested;
        }
      if (now != G_MAXUINT)
        cs->portrait_timestamp = now;
    }
}
