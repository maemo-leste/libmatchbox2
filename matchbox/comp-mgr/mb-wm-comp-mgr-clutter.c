/*
 *  Matchbox Window Manager - A lightweight window manager not for the
 *                            desktop.
 *
 *  Authored By Tomas Frydrych <tf@o-hand.com>
 *
 *  Copyright (c) 2008 OpenedHand Ltd - http://o-hand.com
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

#ifndef HAVE_CLUTTER_EGLX
/* Gordon says: */
#define HAVE_CLUTTER_EGLX 0
#endif

#define SGX_CORRUPTION_WORKAROUND 1

#include "mb-wm.h"
#include "mb-wm-client.h"
#include "mb-wm-comp-mgr.h"
#include "mb-wm-comp-mgr-clutter.h"
#include "mb-wm-theme.h"

#include <clutter/clutter.h>
#include <clutter/x11/clutter-x11.h>
#if HAVE_CLUTTER_GLX
#include <clutter/glx/clutter-glx-texture-pixmap.h>
#elif HAVE_CLUTTER_EGLX
#include <clutter/clutter-eglx-texture-pixmap.h>
#endif
#include <X11/Xresource.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/Xcomposite.h>

#include <math.h>

#define SHADOW_RADIUS 4
#define SHADOW_OPACITY	0.9
#define SHADOW_OFFSET_X	(-SHADOW_RADIUS)
#define SHADOW_OFFSET_Y	(-SHADOW_RADIUS)

#define MAX_TILE_SZ 16 	/* make sure size/2 < MAX_TILE_SZ */
#define WIDTH  (3*MAX_TILE_SZ)
#define HEIGHT (3*MAX_TILE_SZ)

#if SGX_CORRUPTION_WORKAROUND
/* FIXME: This is copied from hd-wm, and should be removed
 * when we take out the nasty X11 hack */
typedef enum _HdWmClientType
{
  HdWmClientTypeHomeApplet  = MBWMClientTypeLast << 1,
  HdWmClientTypeAppMenu     = MBWMClientTypeLast << 2,
  HdWmClientTypeStatusArea  = MBWMClientTypeLast << 3,
  HdWmClientTypeStatusMenu  = MBWMClientTypeLast << 4,
  HdWmClientTypeAnimationActor = MBWMClientTypeLast << 5,
} HdWmClientType;
#endif

static void
mb_wm_comp_mgr_clutter_add_actor (MBWMCompMgrClutter *,
				  MBWMCompMgrClutterClient *);

/**
 * Implementation of MBWMCompMgrClutterClient.
 */
struct MBWMCompMgrClutterClientPrivate
{
  ClutterActor          * actor;  /* Overall actor */
  ClutterActor          * texture; /* The texture part of our actor */
  Pixmap                  pixmap;
  int                     pxm_width;
  int                     pxm_height;
  int                     pxm_depth;
  unsigned int            flags;
  Bool                    fullscreen;
  Damage                  frame_damage;
  Damage                  window_damage;
};

static void
mb_wm_comp_mgr_clutter_client_show_real (MBWMCompMgrClient * client);

static void
mb_wm_comp_mgr_clutter_client_hide_real (MBWMCompMgrClient * client);

static void
mb_wm_comp_mgr_clutter_client_repair_real (MBWMCompMgrClient *client,
                                           Damage damage);

static void
mb_wm_comp_mgr_clutter_client_configure_real (MBWMCompMgrClient * client);

static void
mb_wm_comp_mgr_clutter_client_class_init (MBWMObjectClass *klass)
{
  MBWMCompMgrClientClass *c_klass = MB_WM_COMP_MGR_CLIENT_CLASS (klass);

  c_klass->show       = mb_wm_comp_mgr_clutter_client_show_real;
  c_klass->hide       = mb_wm_comp_mgr_clutter_client_hide_real;
  c_klass->repair     = mb_wm_comp_mgr_clutter_client_repair_real;
  c_klass->configure  = mb_wm_comp_mgr_clutter_client_configure_real;

#if MBWM_WANT_DEBUG
  klass->klass_name = "MBWMCompMgrClutterClient";
#endif
}

/**
 * Fetch the entire texture for our client
 */
static void
mb_wm_comp_mgr_clutter_fetch_texture (MBWMCompMgrClient *client)
{
  MBWMCompMgrClutterClient  *cclient  = MB_WM_COMP_MGR_CLUTTER_CLIENT(client);
  MBWindowManagerClient     *wm_client = client->wm_client;
  MBWindowManager           *wm        = client->wm;
  MBGeometry                 geom;
  Window                     xwin;
  Window root;
  int                        x, y;
  unsigned int               w, h, bw, depth;
#ifdef HAVE_XEXT
  /* Stuff we need for shaped windows */
  XRectangle                *shp_rect;
  int                        shp_order;
  int                        shp_count;
  int                        i;
#endif

  if (!(cclient->priv->flags & MBWMCompMgrClutterClientMapped))
    return;

  Bool fullscreen;

  fullscreen = mb_wm_client_window_is_state_set (
      wm_client->window, MBWMClientWindowEWMHStateFullscreen);

  xwin =
    wm_client->xwin_frame && !fullscreen ?
      wm_client->xwin_frame : wm_client->window->xwindow;

  mb_wm_util_trap_x_errors ();
  XSync (wm->xdpy, False);

  if (cclient->priv->pixmap)
    XFreePixmap (wm->xdpy, cclient->priv->pixmap);

  cclient->priv->pixmap = XCompositeNameWindowPixmap (wm->xdpy, xwin);

  if (!cclient->priv->pixmap)
    {
      mb_wm_util_untrap_x_errors ();
      return;
    }

  XGetGeometry (wm->xdpy, cclient->priv->pixmap, &root,
		&x, &y, &w, &h, &bw, &depth);

  mb_wm_client_get_coverage (wm_client, &geom);

  cclient->priv->pxm_width  = w;
  cclient->priv->pxm_height = h;
  cclient->priv->pxm_depth  = depth;

  /*
  g_debug ("%s: x: %d/%d y: %d/%d w: %d/%d h: %d/%d", __FUNCTION__,
           geom.x, x, geom.y, y,
           geom.width, w, geom.height, h);
           */

  if (!(cclient->priv->flags & MBWMCompMgrClutterClientDontPosition))
    {
      clutter_actor_set_position (cclient->priv->actor, geom.x, geom.y);
    }
  clutter_actor_set_size (cclient->priv->texture, geom.width, geom.height);

  /* this will also cause updating the corresponding pixmap
   * and ensures window<->pixmap binding */
  clutter_x11_texture_pixmap_set_window (
        CLUTTER_X11_TEXTURE_PIXMAP (cclient->priv->texture),
        xwin, FALSE);

  if (mb_wm_util_untrap_x_errors () == BadDrawable)
    {
      g_debug ("%s: BadDrawable for %lx", __FUNCTION__, cclient->priv->pixmap);
      cclient->priv->pixmap = None;
      return;
    }

#ifdef HAVE_XEXT
  /*
   * If the client is shaped, we have to tell our texture about which bits of
   * it are visible. If it's not we want to just clear all shapes, and it'll
   * know it needs to draw the whole thing
   */
  clutter_x11_texture_pixmap_clear_shapes(
                    CLUTTER_X11_TEXTURE_PIXMAP (cclient->priv->texture));

  if (mb_wm_theme_is_client_shaped (wm->theme, wm_client))
    {
      shp_rect = XShapeGetRectangles (wm->xdpy, xwin,
				     ShapeBounding, &shp_count, &shp_order);

      if (shp_rect && shp_count)
	{
	  for (i = 0; i < shp_count; ++i)
	    {
	      ClutterGeometry geo;
	      geo.x = shp_rect[i].x;
	      geo.y = shp_rect[i].y;
	      geo.width = shp_rect[i].width;
	      geo.height = shp_rect[i].height;

	      clutter_x11_texture_pixmap_add_shape(
	          CLUTTER_X11_TEXTURE_PIXMAP (cclient->priv->texture),
	          geo);
	    }

	  XFree (shp_rect);
	}
    }

#endif
}

static int
mb_wm_comp_mgr_clutter_client_init (MBWMObject *obj, va_list vap)
{
  MBWMCompMgrClutterClient *cclient = MB_WM_COMP_MGR_CLUTTER_CLIENT (obj);

  cclient->priv =
    mb_wm_util_malloc0 (sizeof (MBWMCompMgrClutterClientPrivate));

  cclient->priv->actor = clutter_group_new();

  return 1;
}

static void
mb_wm_comp_mgr_clutter_client_destroy (MBWMObject* obj)
{
  MBWMCompMgrClient        * c   = MB_WM_COMP_MGR_CLIENT (obj);
  MBWMCompMgrClutterClient * cclient = MB_WM_COMP_MGR_CLUTTER_CLIENT (obj);
  MBWindowManager          * wm  = c->wm;

  if (cclient->priv->actor)
    clutter_actor_destroy (cclient->priv->actor);

  if (cclient->priv->pixmap)
    XFreePixmap (wm->xdpy, cclient->priv->pixmap);

  if (cclient->priv->frame_damage)
    XDamageDestroy (wm->xdpy, cclient->priv->frame_damage);
  if (cclient->priv->window_damage)
    XDamageDestroy (wm->xdpy, cclient->priv->window_damage);

  free (cclient->priv);
}

int
mb_wm_comp_mgr_clutter_client_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (MBWMCompMgrClutterClientClass),
	sizeof (MBWMCompMgrClutterClient),
	mb_wm_comp_mgr_clutter_client_init,
	mb_wm_comp_mgr_clutter_client_destroy,
	mb_wm_comp_mgr_clutter_client_class_init
      };

      type =
	mb_wm_object_register_class (&info, MB_WM_TYPE_COMP_MGR_CLIENT, 0);
    }

  return type;
}

/**
 * This is a private method, hence static (all instances of this class are
 * created automatically by the composite manager).
 */
static MBWMCompMgrClient *
mb_wm_comp_mgr_clutter_client_new (MBWindowManagerClient * client)
{
  MBWMObject *c;

  c = mb_wm_object_new (MB_WM_TYPE_COMP_MGR_CLUTTER_CLIENT,
			MBWMObjectPropClient, client,
			NULL);

  return MB_WM_COMP_MGR_CLIENT (c);
}

static void
mb_wm_comp_mgr_clutter_client_hide_real (MBWMCompMgrClient * client)
{
  MBWMCompMgrClutterClient * cclient = MB_WM_COMP_MGR_CLUTTER_CLIENT (client);

  /*
   * Do not hide the actor if effect is in progress
   */
  if (cclient->priv->flags & MBWMCompMgrClutterClientEffectRunning)
    return;

  clutter_actor_hide (cclient->priv->actor);
}

static void
mb_wm_comp_mgr_clutter_client_show_real (MBWMCompMgrClient * client)
{
  MBWMCompMgrClutterClient * cclient = MB_WM_COMP_MGR_CLUTTER_CLIENT (client);

  if (!cclient->priv->actor)
    {
      /*
       * This can happen if show() is called on our client before it is
       * actually mapped (we only alocate the actor in response to map
       * notification.
       */
      return;
    }

  /*
   * Clear the don't update flag, if set
   */
  cclient->priv->flags &= ~MBWMCompMgrClutterClientDontUpdate;

  if (!(cclient->priv->flags & MBWMCompMgrClutterClientDontShow))
    clutter_actor_show (cclient->priv->actor);
}

void
mb_wm_comp_mgr_clutter_client_set_flags (MBWMCompMgrClutterClient     *cclient,
					 MBWMCompMgrClutterClientFlags flags)
{
  cclient->priv->flags |= flags;
}


void
mb_wm_comp_mgr_clutter_client_unset_flags (MBWMCompMgrClutterClient  *cclient,
					   MBWMCompMgrClutterClientFlags flags)
{
  cclient->priv->flags &= ~flags;
}

MBWMCompMgrClutterClientFlags
mb_wm_comp_mgr_clutter_client_get_flags (MBWMCompMgrClutterClient  *cclient)
{
  return (MBWMCompMgrClutterClientFlags) cclient->priv->flags;
}

ClutterActor *
mb_wm_comp_mgr_clutter_client_get_actor (MBWMCompMgrClutterClient *cclient)
{ /* Don't try to g_object_ref(NULL), which is when we're unmapped. */
  return cclient->priv->actor ? g_object_ref (cclient->priv->actor) : NULL;
}


/**
 * Implementation of MBWMCompMgrClutter
 */
struct MBWMCompMgrClutterPrivate
{
  ClutterActor * arena;
  MBWMList     * desktops;

  Window         overlay_window;
};

static void
mb_wm_comp_mgr_clutter_private_free (MBWMCompMgrClutter *mgr)
{
  MBWMCompMgrClutterPrivate * priv = mgr->priv;

  free (priv);
}

static void
mb_wm_comp_mgr_clutter_register_client_real (MBWMCompMgr           * mgr,
					     MBWindowManagerClient * c)
{
  MBWMCompMgrClient       *cclient;
  MBWMCompMgrClutterClass *klass
    = MB_WM_COMP_MGR_CLUTTER_CLASS (MB_WM_OBJECT_GET_CLASS (mgr));

  if (c->cm_client)
    return;

  cclient = klass->client_new (c);
  c->cm_client = cclient;
}

static void
mb_wm_comp_mgr_clutter_turn_on_real (MBWMCompMgr *mgr);

static void
mb_wm_comp_mgr_clutter_turn_off_real (MBWMCompMgr *mgr);

static void
mb_wm_comp_mgr_clutter_screen_size_changed (MBWMCompMgr *mgr,
                                            unsigned w, unsigned h);

static void
mb_wm_comp_mgr_clutter_map_notify_real (MBWMCompMgr *mgr,
					MBWindowManagerClient *c);

static void
mb_wm_comp_mgr_clutter_restack_real (MBWMCompMgr *mgr);

static Bool
mb_wm_comp_mgr_is_my_window_real (MBWMCompMgr * mgr, Window xwin);

static void
mb_wm_comp_mgr_clutter_select_desktop (MBWMCompMgr * mgr,
				       int desktop, int old_desktop);

static Bool
mb_wm_comp_mgr_clutter_handle_damage (XDamageNotifyEvent * de,
				      MBWMCompMgr        * mgr);

static void
mb_wm_comp_mgr_clutter_class_init (MBWMObjectClass *klass)
{
  MBWMCompMgrClass        *cm_klass = MB_WM_COMP_MGR_CLASS (klass);
  MBWMCompMgrClutterClass *clutter_klass =
    MB_WM_COMP_MGR_CLUTTER_CLASS (klass);

#if MBWM_WANT_DEBUG
  klass->klass_name = "MBWMCompMgrClutter";
#endif

  /*
   * NB -- we do not need render() implementation, since that is taken care of
   * automatically by clutter stage.
   */
  cm_klass->register_client   = mb_wm_comp_mgr_clutter_register_client_real;
  cm_klass->turn_on           = mb_wm_comp_mgr_clutter_turn_on_real;
  cm_klass->turn_off          = mb_wm_comp_mgr_clutter_turn_off_real;
  cm_klass->map_notify        = mb_wm_comp_mgr_clutter_map_notify_real;
  cm_klass->my_window         = mb_wm_comp_mgr_is_my_window_real;
  cm_klass->restack           = mb_wm_comp_mgr_clutter_restack_real;
  cm_klass->select_desktop    = mb_wm_comp_mgr_clutter_select_desktop;
  cm_klass->handle_damage     = mb_wm_comp_mgr_clutter_handle_damage;
  cm_klass->screen_size_changed = mb_wm_comp_mgr_clutter_screen_size_changed;

  clutter_klass->client_new   = mb_wm_comp_mgr_clutter_client_new;
}

static int
mb_wm_comp_mgr_clutter_init (MBWMObject *obj, va_list vap)
{
  MBWMCompMgr                * mgr  = MB_WM_COMP_MGR (obj);
  MBWMCompMgrClutter         * cmgr = MB_WM_COMP_MGR_CLUTTER (obj);
  MBWMCompMgrClutterPrivate  * priv;
  MBWindowManager            * wm = mgr->wm;
  ClutterActor               * desktop, * arena;

  priv = mb_wm_util_malloc0 (sizeof (MBWMCompMgrClutterPrivate));
  cmgr->priv = priv;

  XCompositeRedirectSubwindows (wm->xdpy, wm->root_win->xwindow,
				CompositeRedirectManual);

  priv->arena = arena = clutter_group_new ();
  clutter_actor_set_name (priv->arena, "MBWMCompMgrClutter:arena");
  clutter_container_add_actor (CLUTTER_CONTAINER (clutter_stage_get_default()),
			       arena);
  clutter_actor_show (arena);

  desktop = clutter_group_new ();
  clutter_actor_set_name (desktop, "desktop");
  clutter_actor_show (desktop);
  clutter_container_add_actor (CLUTTER_CONTAINER (arena), desktop);
  priv->desktops = mb_wm_util_list_append (priv->desktops, desktop);

  return 1;
}

static void
mb_wm_comp_mgr_clutter_destroy (MBWMObject * obj)
{
  MBWMCompMgr        * mgr  = MB_WM_COMP_MGR (obj);
  MBWMCompMgrClutter * cmgr = MB_WM_COMP_MGR_CLUTTER (obj);

  mb_wm_comp_mgr_turn_off (mgr);
  mb_wm_comp_mgr_clutter_private_free (cmgr);
}

int
mb_wm_comp_mgr_clutter_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (MBWMCompMgrClutterClass),
	sizeof (MBWMCompMgrClutter),
	mb_wm_comp_mgr_clutter_init,
	mb_wm_comp_mgr_clutter_destroy,
	mb_wm_comp_mgr_clutter_class_init
      };

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_COMP_MGR, 0);
    }

  return type;
}

/* Shuts the compositing down */
static void
mb_wm_comp_mgr_clutter_turn_off_real (MBWMCompMgr *mgr)
{
  MBWindowManager            *wm;
  MBWMCompMgrClutterPrivate  *priv;

  if (!mgr)
    return;

  wm = mgr->wm;
  priv = MB_WM_COMP_MGR_CLUTTER (mgr)->priv;

  if (mgr->disabled)
    return;

  if (!mb_wm_stack_empty (wm))
    {
      MBWindowManagerClient * c;

      mb_wm_stack_enumerate (wm, c)
	{
	  mb_wm_comp_mgr_unregister_client (mgr, c);
	}
    }

  XCompositeReleaseOverlayWindow (wm->xdpy, wm->root_win->xwindow);
  priv->overlay_window = None;

  mgr->disabled = True;
}

static void
mb_wm_comp_mgr_clutter_turn_on_real (MBWMCompMgr *mgr)
{
  MBWindowManager            * wm;
  MBWMCompMgrClutterPrivate  * priv;

  if (!mgr || !mgr->disabled)
    return;

  priv = MB_WM_COMP_MGR_CLUTTER (mgr)->priv;
  wm = mgr->wm;

  mgr->disabled = False;

  if (priv->overlay_window == None)
    {
      ClutterActor    * stage = clutter_stage_get_default ();
      ClutterColor      clr = {0, 0, 0, 0xff };
      Window            xwin;
      XserverRegion     region;

      /*
       * Fetch the overlay window
       */
      xwin = clutter_x11_get_stage_window (CLUTTER_STAGE (stage));

      /* Make sure the overlay window's size is the same as the screen's
       * actual size.  Necessary if the screen is rotated. */
      priv->overlay_window =
	XCompositeGetOverlayWindow (wm->xdpy, wm->root_win->xwindow);
      XResizeWindow (wm->xdpy, priv->overlay_window,
                     wm->xdpy_width, wm->xdpy_height);

      /*
       * Reparent the stage window to the overlay window, this makes it
       * magically work :)
       */
      XReparentWindow (wm->xdpy, xwin, priv->overlay_window, 0, 0);

      /*
       * Use xfixes shape to make events pass through the overlay window
       *
       * TODO -- this has certain drawbacks, notably when our client is
       * tranformed (rotated, scaled, etc), the events will not be landing in
       * the right place. The answer to that is event forwarding with
       * translation.
       */
      region = XFixesCreateRegion (wm->xdpy, NULL, 0);

      XFixesSetWindowShapeRegion (wm->xdpy, priv->overlay_window,
				  ShapeBounding, 0, 0, 0);
      XFixesSetWindowShapeRegion (wm->xdpy, priv->overlay_window,
				  ShapeInput, 0, 0, region);

      XFixesDestroyRegion (wm->xdpy, region);

      clutter_actor_set_size (stage, wm->xdpy_width, wm->xdpy_height);
      clutter_stage_set_color (CLUTTER_STAGE (stage), &clr);

      clutter_actor_show (stage);
    }
}

/* Synchronize the size of the stage and the overlay window
 * with the root window. */
static void
mb_wm_comp_mgr_clutter_screen_size_changed (MBWMCompMgr *mgr,
                                            unsigned w, unsigned h)
{
  MBWMCompMgrClutterPrivate *priv = MB_WM_COMP_MGR_CLUTTER (mgr)->priv;

  clutter_actor_set_size (clutter_stage_get_default (), w, h);
  XResizeWindow (mgr->wm->xdpy, priv->overlay_window, w, h);
}

static void
mb_wm_comp_mgr_clutter_client_repair_real (MBWMCompMgrClient *client,
                                           Damage damage)
{
  MBWMCompMgrClutterClient * cclient = MB_WM_COMP_MGR_CLUTTER_CLIENT (client);
  MBWindowManager          * wm   = client->wm;
  XserverRegion              parts;
  int                        r_count;
  XRectangle               * r_damage;
  XRectangle                 r_bounds;

//  MBWM_NOTE (COMPOSITOR, "REPAIRING %lx", wm_client->window->xwindow);

  if (!cclient->priv->actor)
    return;

  if (!cclient->priv->pixmap)
    {
      /*
       * First time we have been called since creation/configure,
       * fetch the whole texture.
      printf ("%s: Full drawable repair\n", __FUNCTION__);
       */
      XDamageSubtract (wm->xdpy, damage, None, None);
      mb_wm_comp_mgr_clutter_fetch_texture (client);
      return;
    }

  /*
   * Retrieve the damaged region and break it down into individual
   * rectangles so we do not have to update the whole shebang.
   */
  parts = XFixesCreateRegion (wm->xdpy, 0, 0);
  XDamageSubtract (wm->xdpy, damage, None, parts);

  r_damage = XFixesFetchRegionAndBounds (wm->xdpy, parts,
					 &r_count,
					 &r_bounds);

  if (r_damage)
    {
      clutter_x11_texture_pixmap_update_area (
			CLUTTER_X11_TEXTURE_PIXMAP (cclient->priv->texture),
			r_bounds.x, r_bounds.y,
                        r_bounds.width, r_bounds.height);
      XFree (r_damage);
    }

  XFixesDestroyRegion (wm->xdpy, parts);
}

static void
mb_wm_comp_mgr_clutter_client_configure_real (MBWMCompMgrClient * client)
{
  MBWindowManagerClient    * wm_client = client->wm_client;
  MBWMCompMgrClutterClient * cclient = MB_WM_COMP_MGR_CLUTTER_CLIENT (client);
  Bool                       fullscreen;
  MBWMClientType             ctype = MB_WM_CLIENT_CLIENT_TYPE (wm_client);

  /*
   * We used to release the backing pixmap here, but there seems no point
   * as it will be freed by _fetch_texture anyway - and it was causing
   * some out our pixmaps to black out in the task switcher.
   */

  /*
   * Here we check if the full-screen status of the client has been changed
   * since the last configure event.
   */
  fullscreen = mb_wm_client_window_is_state_set (
      wm_client->window, MBWMClientWindowEWMHStateFullscreen);

  if (fullscreen != cclient->priv->fullscreen)
    {
      cclient->priv->fullscreen = fullscreen;

      mb_wm_comp_mgr_clutter_fetch_texture (client);
    }

  /* Detect if the X size or position is different to our size and position
   * and re-adjust */
  if (cclient->priv->actor && cclient->priv->texture)
    {
      MBGeometry geom;
      gint x, y;
      guint width, height;
      mb_wm_client_get_coverage (client->wm_client, &geom);
      clutter_actor_get_position(cclient->priv->actor, &x, &y);
      clutter_actor_get_size(cclient->priv->texture, &width, &height);
      if (geom.x != x ||
          geom.y != y ||
          geom.width != width ||
          geom.height != height)
        {
          if (!(cclient->priv->flags & MBWMCompMgrClutterClientDontPosition))
	    {
              clutter_actor_set_position(cclient->priv->actor, geom.x, geom.y);
	    }
          clutter_actor_set_size(cclient->priv->texture,
                            geom.width, geom.height);
          /*
          g_debug("%s: Position Changed : %d, %d, %d, %d -> %d, %d, %d, %d",
              __FUNCTION__, x,y,width,height,
              geom.x, geom.y, geom.width, geom.height);
              */
        }
    }
}

static Bool
mb_wm_comp_mgr_clutter_handle_damage (XDamageNotifyEvent * de,
				      MBWMCompMgr        * mgr)
{
  MBWindowManager           * wm   = mgr->wm;
  MBWindowManagerClient     * c;
  Damage                      damage;

  c = mb_wm_managed_client_from_frame (wm, de->drawable);

  if (c && c->cm_client)
    {
      MBWMCompMgrClutterClient *cclient =
	MB_WM_COMP_MGR_CLUTTER_CLIENT (c->cm_client);

      if (!cclient->priv->actor ||
	  (cclient->priv->flags & MBWMCompMgrClutterClientDontUpdate))
	return False;

      MBWM_NOTE (COMPOSITOR,
		 "Repairing window %lx, geometry %d,%d;%dx%d; more %d\n",
		 de->drawable,
		 de->geometry.x,
		 de->geometry.y,
		 de->geometry.width,
		 de->geometry.height,
		 de->more);
      /*
       * In full-screen mode we are not watching the frame window. When the
       * full-screen mode is set we only watch the frame window.
       */
      damage = ((XEvent *)de)->xany.window == c->xwin_frame ?
	cclient->priv->frame_damage : cclient->priv->window_damage;

      if (((XEvent *)de)->xany.window == c->xwin_frame) {
	if (cclient->priv->fullscreen) {
	  XDamageSubtract (wm->xdpy, damage, None, None);
	  return False;
	}
      } else if (((XEvent *)de)->xany.window == c->window->xwindow) {
	if (!cclient->priv->fullscreen && c->xwin_frame) {
	  XDamageSubtract (wm->xdpy, damage, None, None);
	  return False;
	}
      }

      mb_wm_comp_mgr_clutter_client_repair_real (c->cm_client, damage);
    }
  else
    {
      MBWM_NOTE (COMPOSITOR, "Failed to find client for window %lx\n",
		 de->drawable);
      g_debug ("Failed to find client for window %lx\n",
		 de->drawable);
    }

  return False;
}

static void
mb_wm_comp_mgr_clutter_restack_real (MBWMCompMgr *mgr)
{
  MBWindowManager    * wm = mgr->wm;
  MBWMCompMgrClutter * cmgr = MB_WM_COMP_MGR_CLUTTER (mgr);
  MBWMList           * l;
  int                  i = 0;

  l = cmgr->priv->desktops;

  if (!mb_wm_stack_empty (wm))
    {
      MBWindowManagerClient * c;

      while (l)
	{
	  ClutterActor *desktop = l->data;
	  ClutterActor * prev = NULL;

	  mb_wm_stack_enumerate (wm, c)
	    {
	      MBWMCompMgrClutterClient * cc;
	      ClutterActor             * a;

	      if (mb_wm_client_get_desktop (c) != i)
		continue;

	      cc = MB_WM_COMP_MGR_CLUTTER_CLIENT (c->cm_client);

	      a = cc->priv->actor;

	      if (!a || clutter_actor_get_parent (a) != desktop)
		continue;

	      clutter_actor_raise (a, prev);

	      prev = a;
	    }

	  l = l->next;
	  ++i;
	}
    }
}

MBWMList *
mb_wm_comp_mgr_clutter_get_desktops (MBWMCompMgrClutter *cmgr)
{
  return cmgr->priv->desktops;
}

/*
 * Gets the n-th desktop from our desktop list; if we do not have that many
 * desktops, just append new ones.
 */
ClutterActor *
mb_wm_comp_mgr_clutter_get_nth_desktop (MBWMCompMgrClutter * cmgr, int desktop)
{
  MBWMCompMgrClutterPrivate * priv = cmgr->priv;
  MBWMList * l = priv->desktops;
  int i = 0;

  while (l && i != desktop)
    {
      ++i;

      if (l->next)
	l = l->next;
      else
	{
	  /* End of the line -- append new desktop */
	  ClutterActor * d = clutter_group_new ();
          clutter_actor_set_name (d, "desktop");
	  priv->desktops = mb_wm_util_list_append (priv->desktops, d);
	  clutter_container_add_actor (CLUTTER_CONTAINER (priv->arena), d);

	  l = l->next;
	}
    }
  if (!l)
    return 0;
  return CLUTTER_ACTOR (l->data);
}

/*
 * Returns the arena; this is an intermediate group which contains all the
 * other actors the CM uses. The caller of this function holds a reference
 * to the returned ClutterActor and must release it once no longer needed.
 */
ClutterActor *
mb_wm_comp_mgr_clutter_get_arena (MBWMCompMgrClutter *cmgr)
{
  MBWMCompMgrClutterPrivate * priv = cmgr->priv;

  return g_object_ref (priv->arena);
}


static void
mb_wm_comp_mgr_clutter_select_desktop (MBWMCompMgr * mgr,
				       int           desktop,
				       int           old_desktop)
{
  MBWMCompMgrClutter * cmgr = MB_WM_COMP_MGR_CLUTTER (mgr);
  ClutterActor       * d;
  MBWMList           * l;

  d = mb_wm_comp_mgr_clutter_get_nth_desktop (cmgr, desktop);

  l = cmgr->priv->desktops;

  while (l)
    {
      ClutterActor * a = l->data;

      if (a == d)
	clutter_actor_show (a);
      else
	clutter_actor_hide (a);

      l = l->next;
    }
}

static void
mb_wm_comp_mgr_clutter_map_notify_real (MBWMCompMgr *mgr,
					MBWindowManagerClient *c)
{
  MBWMCompMgrClutter        * cmgr    = MB_WM_COMP_MGR_CLUTTER (mgr);
  MBWMCompMgrClient         * client  = c->cm_client;
  MBWMCompMgrClutterClient  * cclient = MB_WM_COMP_MGR_CLUTTER_CLIENT(client);
  MBWindowManager           * wm      = client->wm;
  ClutterActor              * texture;
  MBGeometry                  geom;
  MBWMClientType              ctype = MB_WM_CLIENT_CLIENT_TYPE (c);
  char                        actor_name[64];

  cclient->priv->fullscreen = mb_wm_client_window_is_state_set (
      c->window, MBWMClientWindowEWMHStateFullscreen);

  if (mb_wm_client_is_hiding_from_desktop (c))
    {
      /*
       * We already have the resources, except we have to get a new
       * backing pixmap
       */
      //Window xwin = c->xwin_frame ? c->xwin_frame : c->window->xwindow;

      /*
       * FIXME -- Must rebind the pixmap to the texture -- this is not ideal
       * since our texture already contains the correct data, but without
       * this it will not update. Perhaps we some extension to the clutter
       * API is needed here.
       */
      mb_wm_comp_mgr_clutter_fetch_texture (client);

      clutter_actor_show (cclient->priv->actor);
      return;
    }

  /*
   * We get called for windows as well as their children, so once we are
   * mapped do nothing.
   */
  if (cclient->priv->flags & MBWMCompMgrClutterClientMapped)
    return;

  cclient->priv->flags |= MBWMCompMgrClutterClientMapped;

  /*
   * In full screen mode the xwin_frame window is unmapped, an the application
   * window (c->window->xwindow) is reparented to the root window. This is why
   * we have to watch both windows using the damage extension.
   *
   * TODO: The title bar of the application window has an other window which we
   * are not watching. This might be the why the title bar is not refreshing.
   */
  if (c->xwin_frame)
    cclient->priv->frame_damage = XDamageCreate (wm->xdpy,
				   c->xwin_frame,
				   XDamageReportNonEmpty);

  cclient->priv->window_damage = XDamageCreate (wm->xdpy,
				   c->window->xwindow,
				   XDamageReportNonEmpty);

  mb_wm_client_get_coverage (c, &geom);

  g_snprintf(actor_name, 64, "window_0x%lx",
             c->xwin_frame ? c->xwin_frame : c->window->xwindow);
  clutter_actor_set_name(cclient->priv->actor, actor_name);

#if HAVE_CLUTTER_GLX
  texture = clutter_glx_texture_pixmap_new ();
#elif HAVE_CLUTTER_EGLX

#if SGX_CORRUPTION_WORKAROUND
  if (ctype == MBWMClientTypeMenu ||
      ctype == MBWMClientTypeNote ||
      ctype == MBWMClientTypeOverride ||
      ctype == HdWmClientTypeStatusArea ||
      ctype == HdWmClientTypeStatusMenu ||
      ctype == HdWmClientTypeHomeApplet ||
      ctype == HdWmClientTypeAppMenu ||
      ctype == HdWmClientTypeAnimationActor
      )
#else
  if (FALSE)
#endif
    {
      /* FIXME: This is a hack to get menus and Status Area working properly
       * until EGL is fixed for strange-sized images. When we remove this,
       * ALSO remove definition of HdWmClientType in this file */
      g_debug ("%s: calling clutter_x11_texture_pixmap_new", __FUNCTION__);
      texture = clutter_x11_texture_pixmap_new ();
    }
  else
    {
      g_debug ("%s: calling clutter_eglx_texture_pixmap_new", __FUNCTION__);
      texture = clutter_eglx_texture_pixmap_new ();
    }
#else
  texture = clutter_x11_texture_pixmap_new ();
#endif
  sprintf(actor_name, "texture_0x%lx",
          c->xwin_frame ? c->xwin_frame : c->window->xwindow);
  clutter_actor_set_name(texture, actor_name);

  clutter_actor_show (texture);

  clutter_container_add_actor (CLUTTER_CONTAINER (cclient->priv->actor), texture);
  /* We want to lower this below any decor */
  clutter_actor_lower_bottom(texture);

  if (cclient->priv->texture)
    clutter_actor_destroy(cclient->priv->texture);
  cclient->priv->texture = texture;

  g_object_set_data (G_OBJECT (cclient->priv->actor),
      "MBWMCompMgrClutterClient", cclient);

  clutter_actor_set_position (cclient->priv->actor, geom.x, geom.y);
  clutter_actor_set_size (texture, geom.width, geom.height);

  /* If the client has a "do not show" flag set explicitly,
     prevent it from being shown when it is added to the desktop */

  if (cclient->priv->flags & MBWMCompMgrClutterClientDontShow)
    g_object_set (cclient->priv->actor, "show-on-set-parent", FALSE, NULL);

  mb_wm_comp_mgr_clutter_add_actor (cmgr, cclient);
}

/*
 * Our windows which we need the WM to ingore are the overlay and the stage
 * window.
 */
static Bool
mb_wm_comp_mgr_is_my_window_real (MBWMCompMgr * mgr, Window xwin)
{
  MBWMCompMgrClutterPrivate * priv = MB_WM_COMP_MGR_CLUTTER (mgr)->priv;
  ClutterActor              * stage;

  if (priv->overlay_window == xwin)
    return True;

  stage = clutter_stage_get_default ();

  if (xwin == clutter_x11_get_stage_window (CLUTTER_STAGE (stage)))
    return True;

  return False;
}

static void
mb_wm_comp_mgr_clutter_add_actor (MBWMCompMgrClutter       * cmgr,
				  MBWMCompMgrClutterClient * cclient)
{
  MBWindowManagerClient * c = MB_WM_COMP_MGR_CLIENT (cclient)->wm_client;
  ClutterActor          * d;
  int                     desktop = mb_wm_client_get_desktop (c);

  /*
   * Sanity check; if the desktop is unset, add to desktop 0.
   */
  if (desktop < 0)
    desktop = 0;

  d = mb_wm_comp_mgr_clutter_get_nth_desktop (cmgr, desktop);

  clutter_container_add_actor (CLUTTER_CONTAINER (d), cclient->priv->actor);
}

MBWMCompMgr *
mb_wm_comp_mgr_clutter_new (MBWindowManager *wm)
{
  MBWMObject *mgr;

  mgr = mb_wm_object_new (MB_WM_TYPE_COMP_MGR_CLUTTER,
			  MBWMObjectPropWm, wm,
			  NULL);

  return MB_WM_COMP_MGR (mgr);
}
