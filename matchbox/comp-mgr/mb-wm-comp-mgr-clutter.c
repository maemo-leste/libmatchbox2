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

/* FIXME: This is copied from hd-wm, and should be removed
 * when we take out the nasty X11 hack */
typedef enum _HdWmClientType
{
  HdWmClientTypeHomeApplet  = MBWMClientTypeLast << 1,
  HdWmClientTypeAppMenu     = MBWMClientTypeLast << 2,
  HdWmClientTypeStatusArea  = MBWMClientTypeLast << 3,
  HdWmClientTypeStatusMenu  = MBWMClientTypeLast << 4,
} HdWmClientType;

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
  clutter_actor_set_position (cclient->priv->actor, geom.x, geom.y);
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
   * it are visible. If it's not we want to just chear all shapes, and it'll
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
  clutter_actor_show_all (cclient->priv->actor);
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


/*
 * MBWMCompMgrClutterClientEventEffect
 */
typedef struct MBWMCompMgrClutterClientEventEffect
{
  ClutterTimeline        *timeline;
  ClutterBehaviour       *behaviour; /* can be either behaviour or effect */
} MBWMCompMgrClutterClientEventEffect;

static void
mb_wm_comp_mgr_clutter_client_event_free (MBWMCompMgrClutterClientEventEffect * effect)
{
  g_object_unref (effect->timeline);
  g_object_unref (effect->behaviour);

  free (effect);
}

/**
 * Data passed to the callback for ClutterTimeline::completed,
 * comp_mgr_clutter_client_event_completed_cb().
 */
struct completed_cb_data
{
  gulong                                my_id;
  MBWMCompMgrClutterClient            * cclient;
  MBWMCompMgrClientEvent                event;
  MBWMCompMgrClutterClientEventEffect * effect;
};

ClutterActor *
mb_wm_comp_mgr_clutter_client_get_actor (MBWMCompMgrClutterClient *cclient)
{ /* Don't try to g_object_ref(NULL), which is when we're unmapped. */
  return cclient->priv->actor ? g_object_ref (cclient->priv->actor) : NULL;
}

static MBWMCompMgrClutterClientEventEffect *
mb_wm_comp_mgr_clutter_client_event_new (MBWMCompMgrClient     *client,
					 MBWMCompMgrClientEvent event,
					 unsigned long          duration)
{
  MBWMCompMgrClutterClientEventEffect * eff;
  ClutterTimeline          * timeline;
  ClutterBehaviour         * behaviour = NULL;
  ClutterAlpha             * alpha;
  MBWMCompMgrClutterClient * cclient = MB_WM_COMP_MGR_CLUTTER_CLIENT (client);
  MBWindowManager          * wm = client->wm;
  ClutterKnot                knots[2];
  MBGeometry                 geom;

  if (!cclient->priv->actor)
    return NULL;

  timeline = clutter_timeline_new_for_duration (duration);

  if (!timeline)
    return NULL;

  alpha = clutter_alpha_new_full (timeline,
				  CLUTTER_ALPHA_RAMP_INC, NULL, NULL);

  mb_wm_client_get_coverage (client->wm_client, &geom);

  switch (event)
    {
    case MBWMCompMgrClientEventMinimize:
      behaviour =
	clutter_behaviour_scale_newx (alpha, CFX_ONE, CFX_ONE, 0, 0);
      break;
    case MBWMCompMgrClientEventUnmap:
      behaviour = clutter_behaviour_opacity_new (alpha, 0xff, 0);
      break;
    case MBWMCompMgrClientEventMap:
      knots[0].x = -wm->xdpy_width;
      knots[0].y = geom.y;
      knots[1].x = geom.x;
      knots[1].y = geom.y;
      behaviour = clutter_behaviour_path_new (alpha, &knots[0], 2);
      break;
    default:
      g_warning ("%s: unhandled switch case!", __FUNCTION__);
    }

  eff = mb_wm_util_malloc0 (sizeof (MBWMCompMgrClutterClientEventEffect));
  eff->timeline = timeline;
  eff->behaviour = behaviour;

  clutter_behaviour_apply (behaviour, cclient->priv->actor);

  return eff;
}

/**
 * Implementation of MBWMCompMgrClutter
 */
struct MBWMCompMgrClutterPrivate
{
  ClutterActor * arena;
  MBWMList     * desktops;
  ClutterActor * shadow;

  Window         overlay_window;
};

static void
mb_wm_comp_mgr_clutter_private_free (MBWMCompMgrClutter *mgr)
{
  MBWMCompMgrClutterPrivate * priv = mgr->priv;

  if (priv->shadow)
    clutter_actor_destroy (priv->shadow);

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
mb_wm_comp_mgr_clutter_map_notify_real (MBWMCompMgr *mgr,
					MBWindowManagerClient *c);

static void
mb_wm_comp_mgr_clutter_client_transition_real (MBWMCompMgr * mgr,
                                               MBWindowManagerClient *c1,
                                               MBWindowManagerClient *c2,
                                               Bool reverse);

static void
mb_wm_comp_mgr_clutter_client_event_real (MBWMCompMgr            * mgr,
                                          MBWindowManagerClient  * client,
                                          MBWMCompMgrClientEvent   event);

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
  cm_klass->client_transition = mb_wm_comp_mgr_clutter_client_transition_real;
  cm_klass->client_event      = mb_wm_comp_mgr_clutter_client_event_real;
  cm_klass->restack           = mb_wm_comp_mgr_clutter_restack_real;
  cm_klass->select_desktop    = mb_wm_comp_mgr_clutter_select_desktop;
  cm_klass->handle_damage     = mb_wm_comp_mgr_clutter_handle_damage;

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
  MBWindowManager            * wm = mgr->wm;
  MBWMCompMgrClutterPrivate  * priv;

  if (!mgr)
    return;

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

      priv->overlay_window =
	XCompositeGetOverlayWindow (wm->xdpy, wm->root_win->xwindow);

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

static void
mb_wm_comp_mgr_clutter_client_repair_real (MBWMCompMgrClient *client,
                                           Damage damage)
{
  MBWMCompMgrClutterClient * cclient = MB_WM_COMP_MGR_CLUTTER_CLIENT (client);
  MBWindowManager          * wm   = client->wm;
  XserverRegion              parts;
  int                        i, r_count;
  XRectangle               * r_damage;
  XRectangle                 r_bounds;

  MBWM_NOTE (COMPOSITOR, "REPAIRING %lx", wm_client->window->xwindow);

  if (!cclient->priv->actor)
    return;

  if (!cclient->priv->pixmap)
    {
      /*
       * First time we have been called since creation/configure,
       * fetch the whole texture.
       */
      printf ("%s: Full drawable repair\n", __FUNCTION__);
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

      /* FIXME xmas fix -- ignore the invisible systemui dialog, because it
       * would show as white window for some reason */
      if (!(ctype == MBWMClientTypeMenu && wm_client->window &&
          g_strcmp0 ("systemui", wm_client->window->name) == 0))
        {
          mb_wm_comp_mgr_clutter_fetch_texture (client);
        }
      else
        g_debug("%s: skip systemui dialog", __FUNCTION__);
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
          clutter_actor_set_position(cclient->priv->actor, geom.x, geom.y);
          clutter_actor_set_size(cclient->priv->texture,
                            geom.width, geom.height);
          g_debug("%s: Position Changed : %d, %d, %d, %d -> %d, %d, %d, %d",
              __FUNCTION__, x,y,width,height,
              geom.x, geom.y, geom.width, geom.height);
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
  ClutterActor              * actor;
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

  actor = g_object_ref (clutter_group_new ());

  if (c->name)
    g_snprintf(actor_name, 64, "window_%s", c->name);
  else
    g_snprintf(actor_name, 64, "window_0x%lx",
            c->xwin_frame ? c->xwin_frame : c->window->xwindow);
  clutter_actor_set_name(actor, actor_name);

#if HAVE_CLUTTER_GLX
  texture = clutter_glx_texture_pixmap_new ();
#elif HAVE_CLUTTER_EGLX

#if SGX_CORRUPTION_WORKAROUND
  if (ctype == MBWMClientTypeMenu ||
      ctype == MBWMClientTypeNote ||
      ctype == HdWmClientTypeStatusArea ||
      ctype == HdWmClientTypeStatusMenu ||
      ctype == HdWmClientTypeHomeApplet ||
      ctype == HdWmClientTypeAppMenu
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

  clutter_container_add (CLUTTER_CONTAINER (actor), texture, NULL);

  cclient->priv->actor = actor;
  cclient->priv->texture = texture;

  g_object_set_data (G_OBJECT (actor), "MBWMCompMgrClutterClient", cclient);

  clutter_actor_set_position (actor, geom.x, geom.y);
  clutter_actor_set_size (texture, geom.width, geom.height);

  mb_wm_comp_mgr_clutter_add_actor (cmgr, cclient);
}

/**
 * Structure that gets passed to mb_wm_comp_mgr_clutter_transtion_fade_cb().
 */
struct _fade_cb_data
{
  MBWMCompMgrClutterClient *cclient1;
  MBWMCompMgrClutterClient *cclient2;
  ClutterTimeline  * timeline;
  ClutterBehaviour * beh;
};

static void
mb_wm_comp_mgr_clutter_transtion_fade_cb (ClutterTimeline * t, void * data)
{
  struct _fade_cb_data * d  = data;
  ClutterActor   * a1 = d->cclient1->priv->actor;

  clutter_actor_set_opacity (a1, 0xff);

  d->cclient1->priv->flags &= ~MBWMCompMgrClutterClientEffectRunning;
  d->cclient2->priv->flags &= ~MBWMCompMgrClutterClientEffectRunning;

  mb_wm_object_unref (MB_WM_OBJECT (d->cclient1));
  mb_wm_object_unref (MB_WM_OBJECT (d->cclient2));

  g_object_unref (d->timeline);
  g_object_unref (d->beh);
}

static void
_fade_apply_behaviour_to_client (MBWindowManagerClient * wc,
				 ClutterBehaviour      * b)
{
  MBWMList * l;
  ClutterActor * a = MB_WM_COMP_MGR_CLUTTER_CLIENT (wc->cm_client)->priv->actor;

  clutter_actor_set_opacity (a, 0);
  clutter_behaviour_apply (b, a);

  l = mb_wm_client_get_transients (wc);
  while (l)
    {
      MBWindowManagerClient * c = l->data;

      _fade_apply_behaviour_to_client (c, b);
      l = l->next;
    }
}

static void
mb_wm_comp_mgr_clutter_client_transition_fade (MBWMCompMgrClutterClient *cclient1,
                                               MBWMCompMgrClutterClient *cclient2,
                                               unsigned long duration)
{
  ClutterTimeline             * timeline;
  ClutterAlpha                * alpha;
  static struct _fade_cb_data   cb_data;
  ClutterBehaviour            * b;

  /*
   * Fade is simple -- we only need to animate the second actor and its
   * children, as the stacking order automatically takes care of the
   * actor appearing to fade out from the first one
   */
  timeline = clutter_timeline_new_for_duration (duration);

  alpha = clutter_alpha_new_full (timeline,
				  CLUTTER_ALPHA_RAMP_DEC, NULL, NULL);

  b = clutter_behaviour_opacity_new (alpha, 0xff, 0);

  cb_data.cclient1 = mb_wm_object_ref (MB_WM_OBJECT (cclient1));
  cb_data.cclient2 = mb_wm_object_ref (MB_WM_OBJECT (cclient2));
  cb_data.timeline = timeline;
  cb_data.beh = b;

  _fade_apply_behaviour_to_client (MB_WM_COMP_MGR_CLIENT (cclient2)->wm_client, b);

  /*
   * Must restore the opacity on the 'from' actor
   */
  g_signal_connect (timeline, "completed",
		    G_CALLBACK (mb_wm_comp_mgr_clutter_transtion_fade_cb),
		    &cb_data);

  cclient1->priv->flags |= MBWMCompMgrClutterClientEffectRunning;
  cclient2->priv->flags |= MBWMCompMgrClutterClientEffectRunning;

  clutter_timeline_start (timeline);
}

static void
mb_wm_comp_mgr_clutter_client_transition_real (MBWMCompMgr * mgr,
                                               MBWindowManagerClient *c1,
                                               MBWindowManagerClient *c2,
                                               Bool reverse)
{
  MBWMCompMgrClutterClient * cclient1 =
    MB_WM_COMP_MGR_CLUTTER_CLIENT (c1->cm_client);
  MBWMCompMgrClutterClient * cclient2 =
    MB_WM_COMP_MGR_CLUTTER_CLIENT (c2->cm_client);

  mb_wm_comp_mgr_clutter_client_transition_fade (cclient1,
					         cclient2,
                                                 100);
}

/*
 * Callback for ClutterTimeline::completed signal.
 *
 * One-off; get connected when the timeline is started, and disconnected
 * again when it finishes.
 */
static void
mb_wm_comp_mgr_clutter_client_event_completed_cb (ClutterTimeline * t, void * data)
{
  struct completed_cb_data * d = data;

  d->cclient->priv->flags &= ~MBWMCompMgrClutterClientEffectRunning;

  g_signal_handler_disconnect (t, d->my_id);

  switch (d->event)
    {
    case MBWMCompMgrClientEventUnmap:
    case MBWMCompMgrClientEventMinimize:
      clutter_actor_hide (d->cclient->priv->actor);
      break;

    default:
      break;
    }

  /*
   * Release the extra reference on the CM client that was added for the sake
   * of the effect
   */
  mb_wm_object_unref (MB_WM_OBJECT (d->cclient));

  mb_wm_comp_mgr_clutter_client_event_free (d->effect);

  free (d);
}

static void
mb_wm_comp_mgr_clutter_client_event_real (MBWMCompMgr            * mgr,
					  MBWindowManagerClient  * client,
					  MBWMCompMgrClientEvent   event)
{
  MBWMCompMgrClutterClientEventEffect * eff;
  MBWMCompMgrClutterClient            * cclient =
    MB_WM_COMP_MGR_CLUTTER_CLIENT (client->cm_client);

  if (MB_WM_CLIENT_CLIENT_TYPE (client) != MBWMClientTypeApp)
    return;

  eff = mb_wm_comp_mgr_clutter_client_event_new (client->cm_client,
						 event, 600);

  if (eff)
    {
      ClutterActor *a;
      Bool dont_run = False;

      a = cclient->priv->actor;

      if (CLUTTER_IS_BEHAVIOUR_PATH (eff->behaviour))
	{
	  /*
	   * At this stage, if the actor is not yet visible, move it to
	   * the starting point of the path (this is mostly because of
	   * 'map' effects,  where the clutter_actor_show () is delayed
	   * until this point, so that the actor can be positioned in the
	   * correct location without visible artefacts).
	   *
	   * FIXME -- this is very clumsy; we need clutter API to query
	   * the first knot of the path to avoid messing about with copies
	   * of the list.
	   */

	  GSList * knots =
	    clutter_behaviour_path_get_knots (
				     CLUTTER_BEHAVIOUR_PATH (eff->behaviour));

	  if (knots)
	    {
	      ClutterKnot * k = knots->data;

	      clutter_actor_set_position (a, k->x, k->y);

	      g_slist_free (knots);
	    }
	}

      if (event == MBWMCompMgrClientEventUnmap)
	{
	  cclient->priv->flags |= MBWMCompMgrClutterClientDontUpdate;

	  if (cclient->priv->flags & MBWMCompMgrClutterClientDone)
	    dont_run = True;
	  else
	    cclient->priv->flags |= MBWMCompMgrClutterClientDone;
	}
      else if (event == MBWMCompMgrClientEventMinimize)
	{
	  /*
	   * This is tied specifically to the unmap scale effect (the
	   * themable version of effects allowed to handle this is a nice
	   * generic fashion. :-(
	   */
	  clutter_actor_move_anchor_point_from_gravity (a,
						CLUTTER_GRAVITY_SOUTH_EAST);
	}

      /*
       * Make sure the actor is showing (for example with 'map' effects,
       * the show() is delayed until the effect had chance to
       * set up the actor postion).
       */
      if (!dont_run)
	{
	  struct completed_cb_data * d;

	  d = mb_wm_util_malloc0 (sizeof (struct completed_cb_data));

	  d->cclient = mb_wm_object_ref (MB_WM_OBJECT (cclient));
	  d->event   = event;
	  d->effect  = eff;

	  d->my_id = g_signal_connect (eff->timeline, "completed",
		  G_CALLBACK (mb_wm_comp_mgr_clutter_client_event_completed_cb),
		  d);

	  cclient->priv->flags |= MBWMCompMgrClutterClientEffectRunning;
	  clutter_actor_show (a);
	  clutter_timeline_start (eff->timeline);
	}
      else
	mb_wm_comp_mgr_clutter_client_event_free (eff);
    }
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

/* ------------------------------- */
/* Shadow Generation */

typedef struct MBGaussianMap
{
  int	   size;
  double * data;
} MBGaussianMap;

/*
 * TidyTextureFrame copied from tidy
 */
#include <cogl/cogl.h>

#define TIDY_PARAM_READWRITE    \
        (G_PARAM_READABLE | G_PARAM_WRITABLE | \
         G_PARAM_STATIC_NICK | G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB)

#define TIDY_TYPE_TEXTURE_FRAME (tidy_texture_frame_get_type ())

#define TIDY_TEXTURE_FRAME(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  TIDY_TYPE_TEXTURE_FRAME, TidyTextureFrame))

#define TIDY_TEXTURE_FRAME_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  TIDY_TYPE_TEXTURE_FRAME, TidyTextureFrameClass))

#define TIDY_IS_TEXTURE_FRAME(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  TIDY_TYPE_TEXTURE_FRAME))

#define TIDY_IS_TEXTURE_FRAME_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  TIDY_TYPE_TEXTURE_FRAME))

#define TIDY_TEXTURE_FRAME_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  TIDY_TYPE_TEXTURE_FRAME, TidyTextureFrameClass))

typedef struct _TidyTextureFrame        TidyTextureFrame;
typedef struct _TidyTextureFramePrivate TidyTextureFramePrivate;
typedef struct _TidyTextureFrameClass   TidyTextureFrameClass;

struct _TidyTextureFrame
{
  ClutterCloneTexture              parent;

  /*< priv >*/
  TidyTextureFramePrivate    *priv;
};

struct _TidyTextureFrameClass
{
  ClutterCloneTextureClass parent_class;
};

enum
  {
    PROP_0,
    PROP_LEFT,
    PROP_TOP,
    PROP_RIGHT,
    PROP_BOTTOM
  };

G_DEFINE_TYPE (TidyTextureFrame,
	       tidy_texture_frame,
	       CLUTTER_TYPE_CLONE_TEXTURE);

#define TIDY_TEXTURE_FRAME_GET_PRIVATE(obj)				\
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TIDY_TYPE_TEXTURE_FRAME, TidyTextureFramePrivate))

struct _TidyTextureFramePrivate
{
  gint left, top, right, bottom;
};

static void
tidy_texture_frame_paint (ClutterActor *self)
{
  TidyTextureFramePrivate *priv = TIDY_TEXTURE_FRAME (self)->priv;
  ClutterCloneTexture     *clone_texture = CLUTTER_CLONE_TEXTURE (self);
  ClutterTexture          *parent_texture;
  guint                    width, height;
  guint                    tex_width, tex_height;
  guint                    ex, ey;
  ClutterFixed             tx1, ty1, tx2, ty2;
  ClutterColor             col = { 0xff, 0xff, 0xff, 0xff };
  CoglHandle               cogl_texture;

  priv = TIDY_TEXTURE_FRAME (self)->priv;

  /* no need to paint stuff if we don't have a texture */
  parent_texture = clutter_clone_texture_get_parent_texture (clone_texture);
  if (!parent_texture)
    return;

  /* parent texture may have been hidden, so need to make sure it gets
   * realized
   */
  if (!CLUTTER_ACTOR_IS_REALIZED (parent_texture))
    clutter_actor_realize (CLUTTER_ACTOR (parent_texture));

  cogl_texture = clutter_texture_get_cogl_texture (parent_texture);
  if (cogl_texture == COGL_INVALID_HANDLE)
    return;

  cogl_push_matrix ();

  tex_width  = cogl_texture_get_width (cogl_texture);
  tex_height = cogl_texture_get_height (cogl_texture);

  clutter_actor_get_size (self, &width, &height);

  tx1 = CLUTTER_INT_TO_FIXED (priv->left) / tex_width;
  tx2 = CLUTTER_INT_TO_FIXED (tex_width - priv->right) / tex_width;
  ty1 = CLUTTER_INT_TO_FIXED (priv->top) / tex_height;
  ty2 = CLUTTER_INT_TO_FIXED (tex_height - priv->bottom) / tex_height;

  col.alpha = clutter_actor_get_paint_opacity (self);
  cogl_color (&col);

  ex = width - priv->right;
  if (ex < 0)
    ex = priv->right; 		/* FIXME ? */

  ey = height - priv->bottom;
  if (ey < 0)
    ey = priv->bottom; 		/* FIXME ? */

#define FX(x) CLUTTER_INT_TO_FIXED(x)

  /* top left corner */
  cogl_texture_rectangle (cogl_texture,
                          0,
                          0,
                          FX(priv->left), /* FIXME: clip if smaller */
                          FX(priv->top),
                          0,
                          0,
                          tx1,
                          ty1);

  /* top middle */
  cogl_texture_rectangle (cogl_texture,
                          FX(priv->left),
                          FX(priv->top),
                          FX(ex),
                          0,
                          tx1,
                          0,
                          tx2,
                          ty1);

  /* top right */
  cogl_texture_rectangle (cogl_texture,
                          FX(ex),
                          0,
                          FX(width),
                          FX(priv->top),
                          tx2,
                          0,
                          CFX_ONE,
                          ty1);

  /* mid left */
  cogl_texture_rectangle (cogl_texture,
                          0,
                          FX(priv->top),
                          FX(priv->left),
                          FX(ey),
                          0,
                          ty1,
                          tx1,
                          ty2);

  /* center */
  cogl_texture_rectangle (cogl_texture,
                          FX(priv->left),
                          FX(priv->top),
                          FX(ex),
                          FX(ey),
                          tx1,
                          ty1,
                          tx2,
                          ty2);

  /* mid right */
  cogl_texture_rectangle (cogl_texture,
                          FX(ex),
                          FX(priv->top),
                          FX(width),
                          FX(ey),
                          tx2,
                          ty1,
                          CFX_ONE,
                          ty2);

  /* bottom left */
  cogl_texture_rectangle (cogl_texture,
                          0,
                          FX(ey),
                          FX(priv->left),
                          FX(height),
                          0,
                          ty2,
                          tx1,
                          CFX_ONE);

  /* bottom center */
  cogl_texture_rectangle (cogl_texture,
                          FX(priv->left),
                          FX(ey),
                          FX(ex),
                          FX(height),
                          tx1,
                          ty2,
                          tx2,
                          CFX_ONE);

  /* bottom right */
  cogl_texture_rectangle (cogl_texture,
                          FX(ex),
                          FX(ey),
                          FX(width),
                          FX(height),
                          tx2,
                          ty2,
                          CFX_ONE,
                          CFX_ONE);


  cogl_pop_matrix ();
#undef FX
}


static void
tidy_texture_frame_set_property (GObject      *object,
				 guint         prop_id,
				 const GValue *value,
				 GParamSpec   *pspec)
{
  TidyTextureFrame         *ctexture = TIDY_TEXTURE_FRAME (object);
  TidyTextureFramePrivate  *priv = ctexture->priv;

  switch (prop_id)
    {
    case PROP_LEFT:
      priv->left = g_value_get_int (value);
      break;
    case PROP_TOP:
      priv->top = g_value_get_int (value);
      break;
    case PROP_RIGHT:
      priv->right = g_value_get_int (value);
      break;
    case PROP_BOTTOM:
      priv->bottom = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
tidy_texture_frame_get_property (GObject    *object,
				 guint       prop_id,
				 GValue     *value,
				 GParamSpec *pspec)
{
  TidyTextureFrame *ctexture = TIDY_TEXTURE_FRAME (object);
  TidyTextureFramePrivate  *priv = ctexture->priv;

  switch (prop_id)
    {
    case PROP_LEFT:
      g_value_set_int (value, priv->left);
      break;
    case PROP_TOP:
      g_value_set_int (value, priv->top);
      break;
    case PROP_RIGHT:
      g_value_set_int (value, priv->right);
      break;
    case PROP_BOTTOM:
      g_value_set_int (value, priv->bottom);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
tidy_texture_frame_class_init (TidyTextureFrameClass *klass)
{
  GObjectClass      *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  actor_class->paint = tidy_texture_frame_paint;

  gobject_class->set_property = tidy_texture_frame_set_property;
  gobject_class->get_property = tidy_texture_frame_get_property;

  g_object_class_install_property
    (gobject_class,
     PROP_LEFT,
     g_param_spec_int ("left",
		       "left",
		       "",
		       0, G_MAXINT,
		       0,
		       TIDY_PARAM_READWRITE));

  g_object_class_install_property
    (gobject_class,
     PROP_TOP,
     g_param_spec_int ("top",
		       "top",
		       "",
		       0, G_MAXINT,
		       0,
		       TIDY_PARAM_READWRITE));

  g_object_class_install_property
    (gobject_class,
     PROP_BOTTOM,
     g_param_spec_int ("bottom",
		       "bottom",
		       "",
		       0, G_MAXINT,
		       0,
		       TIDY_PARAM_READWRITE));

  g_object_class_install_property
    (gobject_class,
     PROP_RIGHT,
     g_param_spec_int ("right",
		       "right",
		       "",
		       0, G_MAXINT,
		       0,
		       TIDY_PARAM_READWRITE));

  g_type_class_add_private (gobject_class, sizeof (TidyTextureFramePrivate));
}

static void
tidy_texture_frame_init (TidyTextureFrame *self)
{
  TidyTextureFramePrivate *priv;

  self->priv = priv = TIDY_TEXTURE_FRAME_GET_PRIVATE (self);
}

