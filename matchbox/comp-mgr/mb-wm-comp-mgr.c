/*
 *  Matchbox Window Manager - A lightweight window manager not for the
 *                            desktop.
 *
 *  Authored By Tomas Frydrych <tf@o-hand.com>
 *
 *  Copyright (c) 2002, 2004, 2007 OpenedHand Ltd - http://o-hand.com
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
#include "mb-wm-client.h"
#include "mb-wm-comp-mgr.h"
#include "mb-wm-theme.h"

#include <X11/Xresource.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/Xcomposite.h>

static void
mb_wm_comp_mgr_client_class_init (MBWMObjectClass *klass)
{
#if MBWM_WANT_DEBUG
  klass->klass_name = "MBWMCompMgrClient";
#endif
}

static int
mb_wm_comp_mgr_client_init (MBWMObject *obj, va_list vap)
{
  MBWMCompMgrClient     *client    = MB_WM_COMP_MGR_CLIENT (obj);
  MBWindowManagerClient *wm_client = NULL;
  MBWMObjectProp         prop;
  XRenderPictFormat     *format;

  prop = va_arg(vap, MBWMObjectProp);
  while (prop)
    {
      switch (prop)
	{
	case MBWMObjectPropClient:
	  wm_client = va_arg(vap, MBWindowManagerClient *);
	  break;
	default:
	  MBWMO_PROP_EAT (vap, prop);
	}

      prop = va_arg(vap, MBWMObjectProp);
    }

  if (!wm_client)
    return 0;

  client->wm_client = wm_client;
  client->wm = wm_client->wmref;

  /* Check visual */
  format = XRenderFindVisualFormat (client->wm->xdpy,
				    wm_client->window->visual);

  if (format && format->type == PictTypeDirect && format->direct.alphaMask)
    client->is_argb32 = True;

  return 1;
}

static void
mb_wm_comp_mgr_client_destroy (MBWMObject* obj)
{
}

int
mb_wm_comp_mgr_client_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (MBWMCompMgrClientClass),
	sizeof (MBWMCompMgrClient),
	mb_wm_comp_mgr_client_init,
	mb_wm_comp_mgr_client_destroy,
	mb_wm_comp_mgr_client_class_init
      };

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_OBJECT, 0);
    }

  return type;
}

void
mb_wm_comp_mgr_client_hide (MBWMCompMgrClient * client)
{
  MBWMCompMgrClientClass *klass;

  if (!client)
    return;

  klass = MB_WM_COMP_MGR_CLIENT_CLASS (MB_WM_OBJECT_GET_CLASS (client));

  MBWM_ASSERT (klass->hide != NULL);
  klass->hide (client);
}

void
mb_wm_comp_mgr_client_show (MBWMCompMgrClient * client)
{
  MBWMCompMgrClientClass *klass;

  if (!client)
    return;

  klass = MB_WM_COMP_MGR_CLIENT_CLASS (MB_WM_OBJECT_GET_CLASS (client));

  MBWM_ASSERT (klass->show != NULL);
  klass->show (client);
}

void
mb_wm_comp_mgr_client_configure (MBWMCompMgrClient * client)
{
  MBWMCompMgrClientClass *klass;

  if (!client)
    return;

  klass = MB_WM_COMP_MGR_CLIENT_CLASS (MB_WM_OBJECT_GET_CLASS (client));

  MBWM_ASSERT (klass->configure != NULL);
  klass->configure (client);
}

void
mb_wm_comp_mgr_client_repair (MBWMCompMgrClient * client, Damage damage)
{
  MBWMCompMgrClientClass *klass;

  if (!client)
    return;

  klass = MB_WM_COMP_MGR_CLIENT_CLASS (MB_WM_OBJECT_GET_CLASS (client));

  MBWM_ASSERT (klass->repair != NULL);
  klass->repair (client, damage);
}


/*
 * MBWMCompMgr object
 *
 * Base class for the composite manager, providing the common interface
 * through which the manager is access by the MBWindowManager object.
 */
static void
mb_wm_comp_mgr_class_init (MBWMObjectClass *klass)
{
#if MBWM_WANT_DEBUG
  klass->klass_name = "MBWMCompMgr";
#endif
}

static int
mb_wm_comp_mgr_init (MBWMObject *obj, va_list vap)
{
  MBWMCompMgr           * mgr = MB_WM_COMP_MGR (obj);
  MBWindowManager       * wm  = NULL;
  MBWMObjectProp          prop;

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

  if (!wm)
    return 0;

  mgr->wm       = wm;
  mgr->disabled = True;

  return 1;
}

static void
mb_wm_comp_mgr_destroy (MBWMObject* obj)
{
  MBWMCompMgr * mgr = MB_WM_COMP_MGR (obj);

  mb_wm_comp_mgr_turn_off (mgr);
}

int
mb_wm_comp_mgr_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (MBWMCompMgrClass),
	sizeof (MBWMCompMgr),
	mb_wm_comp_mgr_init,
	mb_wm_comp_mgr_destroy,
	mb_wm_comp_mgr_class_init
      };

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_OBJECT, 0);
    }

  return type;
}


Bool
mb_wm_comp_mgr_enabled (MBWMCompMgr *mgr)
{
  if (!mgr || mgr->disabled)
    return False;

  return True;
}

/*
 * Registers new client with the manager (i.e., when a window is created).
 */
void
mb_wm_comp_mgr_register_client (MBWMCompMgr           * mgr,
				MBWindowManagerClient * client)
{
  MBWMCompMgrClass *klass;

  if (!client)
    return;

  klass = MB_WM_COMP_MGR_CLASS (MB_WM_OBJECT_GET_CLASS (mgr));

  MBWM_ASSERT (klass->register_client != NULL);
  klass->register_client (mgr, client);
}

/*
 * Unregisters existing client (e.g., when window unmaps or is destroyed
 */
void
mb_wm_comp_mgr_unregister_client (MBWMCompMgr           * mgr,
				  MBWindowManagerClient * client)
{
  MBWMCompMgrClass *klass;

  if (!client)
    return;

  klass = MB_WM_COMP_MGR_CLASS (MB_WM_OBJECT_GET_CLASS (mgr));

  if (!client->cm_client)
    return;

  if (klass->unregister_client)
    klass->unregister_client (mgr, client);

  mb_wm_object_unref (MB_WM_OBJECT (client->cm_client));
  client->cm_client = NULL;
}

/*
 * Called to render the entire composited scene on screen.
 */
void
mb_wm_comp_mgr_render (MBWMCompMgr *mgr)
{
  MBWMCompMgrClass *klass;

  if (!mgr)
    return;

  klass  = MB_WM_COMP_MGR_CLASS (MB_WM_OBJECT_GET_CLASS (mgr));

  if (klass->render)
    klass->render (mgr);
}

void
mb_wm_comp_mgr_restack (MBWMCompMgr *mgr)
{
  MBWMCompMgrClass *klass;

  if (!mgr)
    return;

  klass  = MB_WM_COMP_MGR_CLASS (MB_WM_OBJECT_GET_CLASS (mgr));

  if (klass->restack)
    klass->restack (mgr);
}

/*
 * Called when a window we are interested in maps.
 */
void
mb_wm_comp_mgr_map_notify (MBWMCompMgr *mgr, MBWindowManagerClient *c)
{
  MBWMCompMgrClass *klass;

  if (!mgr || !c || !c->cm_client)
    return;

  klass = MB_WM_COMP_MGR_CLASS (MB_WM_OBJECT_GET_CLASS (mgr));

  if (klass->map_notify)
    klass->map_notify (mgr, c);

  /*
   * Run map event effect *before* we call show() on the actor
   * (the effect will take care of showing the actor, and if not, show() gets
   * called by mb_wm_comp_mgr_map_notify()).
   */
  if (!mb_wm_client_is_hiding_from_desktop (c))
    mb_wm_comp_mgr_do_effect (mgr, c, MBWMCompMgrClientEventMap);

  if (c->cm_client)
    mb_wm_comp_mgr_client_show (c->cm_client);
}

void
mb_wm_comp_mgr_unmap_notify (MBWMCompMgr *mgr, MBWindowManagerClient *c)
{
  MBWMCompMgrClass *klass;

  if (!mgr || !c || !c->cm_client)
    return;

  klass = MB_WM_COMP_MGR_CLASS (MB_WM_OBJECT_GET_CLASS (mgr));

  if (!mb_wm_client_is_hiding_from_desktop (c))
    mb_wm_comp_mgr_do_effect (mgr, c, MBWMCompMgrClientEventUnmap);

  if (klass->unmap_notify)
    klass->unmap_notify (mgr, c);

  /*
   * NB: cannot call hide() here, as at this point an effect could be running
   *     -- the subclass needs to take care of this from the effect and/or
   *     unmap_notify() function.
   */
}

/*
 * Runs a transition-effect from client c1 to client c2; the reverse argument
 * indicates notional direction of the transition
 */
void
mb_wm_comp_mgr_do_transition (MBWMCompMgr * mgr,
			      MBWindowManagerClient *c1,
			      MBWindowManagerClient *c2,
			      Bool reverse)
{
  MBWMCompMgrClass *klass;

  /*
   * Transitions can only be done for clients of the same type, so
   * check the types here.
   */
  if (!mgr || !c1 || !c2 ||
      MB_WM_CLIENT_CLIENT_TYPE (c1) != MB_WM_CLIENT_CLIENT_TYPE (c1))
    return;

  klass = MB_WM_COMP_MGR_CLASS (MB_WM_OBJECT_GET_CLASS (mgr));

  if (klass->client_transition)
    klass->client_transition (mgr, c1, c2, reverse);
}

void
mb_wm_comp_mgr_do_effect (MBWMCompMgr            * mgr,
			  MBWindowManagerClient  * client,
			  MBWMCompMgrClientEvent   event)
{
  MBWMCompMgrClass *klass;

  /*
   * Transitions can only be done for clients of the same type, so
   * check the types here.
   */
  if (!mgr || !client)
    return;

  klass = MB_WM_COMP_MGR_CLASS (MB_WM_OBJECT_GET_CLASS (mgr));

  if (klass->client_event)
    klass->client_event (mgr, client, event);
}

void
mb_wm_comp_mgr_select_desktop (MBWMCompMgr * mgr,
			       int           desktop,
			       int           old_desktop)
{
  MBWMCompMgrClass *klass
    = MB_WM_COMP_MGR_CLASS (MB_WM_OBJECT_GET_CLASS (mgr));

  if (klass->select_desktop)
    klass->select_desktop (mgr, desktop, old_desktop);
}

void
mb_wm_comp_mgr_turn_on (MBWMCompMgr *mgr)
{
  MBWindowManager  *wm = mgr->wm;
  MBWMCompMgrClass *klass
    = MB_WM_COMP_MGR_CLASS (MB_WM_OBJECT_GET_CLASS (mgr));

  MBWM_ASSERT (klass->turn_on != NULL);
  klass->turn_on (mgr);

  mgr->damage_cb_id =
    mb_wm_main_context_x_event_handler_add (wm->main_ctx,
					None,
					wm->damage_event_base + XDamageNotify,
					(MBWMXEventFunc)klass->handle_damage,
					mgr);
}

void
mb_wm_comp_mgr_turn_off (MBWMCompMgr *mgr)
{
  MBWindowManager  *wm = mgr->wm;
  MBWMCompMgrClass *klass
    = MB_WM_COMP_MGR_CLASS (MB_WM_OBJECT_GET_CLASS (mgr));

  MBWM_ASSERT (klass->turn_off != NULL);
  klass->turn_off (mgr);

  mb_wm_main_context_x_event_handler_remove (wm->main_ctx,
					wm->damage_event_base + XDamageNotify,
					mgr->damage_cb_id);

  mgr->damage_cb_id = 0;
}

/*
 * Returns true if the window identified by id xwin is a special window
 * that belongs to the compositing manager and should, therefore, be left
 * alone by MBWMWindow manager (e.g., the overalay window).
 */
Bool
mb_wm_comp_mgr_is_my_window (MBWMCompMgr * mgr, Window xwin)
{
  MBWMCompMgrClass *klass
    = MB_WM_COMP_MGR_CLASS (MB_WM_OBJECT_GET_CLASS (mgr));

  if (klass->my_window)
    return klass->my_window (mgr, xwin);

  return False;
}

