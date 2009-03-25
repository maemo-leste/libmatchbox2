/*
 *  Matchbox Window Manager - A lightweight window manager not for the
 *                            desktop.
 *
 *  Authored By Matthew Allum <mallum@o-hand.com>
 *              Tomas Frydrych <tf@o-hand.com>
 *
 *  Copyright (c) 2002, 2004, 2007, 2008 OpenedHand Ltd - http://o-hand.com
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
#include "mb-wm-comp-mgr-xrender.h"

#include <math.h>

#include <X11/Xresource.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/Xcomposite.h>

#define SHADOW_RADIUS 6
#define SHADOW_OPACITY	0.75
#define SHADOW_OFFSET_X	(-SHADOW_RADIUS)
#define SHADOW_OFFSET_Y	(-SHADOW_RADIUS)

/**
 * An MBWMCompMgrClient rendered using XRender.
 */
struct MBWMCompMgrDefaultClient
{
  MBWMCompMgrClient       parent;

  int		          damaged;
  Damage	          damage;
  Picture	          picture;
  XserverRegion	          extents;
  XserverRegion	          border_clip;
};

static void
mb_wm_comp_mgr_xrender_client_show_real (MBWMCompMgrClient * client);

static void
mb_wm_comp_mgr_xrender_client_hide_real (MBWMCompMgrClient * client);

static void
mb_wm_comp_mgr_xrender_client_repair_real (MBWMCompMgrClient * client);

static void
mb_wm_comp_mgr_xrender_client_configure_real (MBWMCompMgrClient * client);

static void
mb_wm_comp_mgr_xrender_client_class_init (MBWMObjectClass *klass)
{
  MBWMCompMgrClientClass *c_klass = MB_WM_COMP_MGR_CLIENT_CLASS (klass);

  c_klass->show      = mb_wm_comp_mgr_xrender_client_show_real;
  c_klass->hide      = mb_wm_comp_mgr_xrender_client_hide_real;
  c_klass->repair    = mb_wm_comp_mgr_xrender_client_repair_real;
  c_klass->configure = mb_wm_comp_mgr_xrender_client_configure_real;

#if MBWM_WANT_DEBUG
  klass->klass_name = "MBWMCompMgrDefaultClient";
#endif
}

static int
mb_wm_comp_mgr_xrender_client_init (MBWMObject *obj, va_list vap)
{
  MBWMCompMgrClient         *client  = MB_WM_COMP_MGR_CLIENT (obj);
  MBWMCompMgrDefaultClient  *dclient = MB_WM_COMP_MGR_DEFAULT_CLIENT (obj);
  MBWindowManager           *wm;
  MBWindowManagerClient     *wm_client = client->wm_client;

  if (!wm_client || !wm_client->wmref)
    return 0;

  return 1;
}

static void
mb_wm_comp_mgr_xrender_client_destroy (MBWMObject* obj)
{
  MBWMCompMgrClient        * c  = MB_WM_COMP_MGR_CLIENT (obj);
  MBWMCompMgrDefaultClient * dc = MB_WM_COMP_MGR_DEFAULT_CLIENT (obj);
  MBWindowManager          * wm = c->wm;

  mb_wm_comp_mgr_client_hide (c);

  if (dc->damage)
    XDamageDestroy (wm->xdpy, dc->damage);

  if (dc->picture)
    XRenderFreePicture (wm->xdpy, dc->picture);

  if (dc->extents)
    XFixesDestroyRegion (wm->xdpy, dc->extents);

  if (dc->border_clip)
    XFixesDestroyRegion (wm->xdpy, dc->border_clip);
}

int
mb_wm_comp_mgr_xrender_client_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (MBWMCompMgrDefaultClientClass),
	sizeof (MBWMCompMgrDefaultClient),
	mb_wm_comp_mgr_xrender_client_init,
	mb_wm_comp_mgr_xrender_client_destroy,
	mb_wm_comp_mgr_xrender_client_class_init
      };

      type =
	mb_wm_object_register_class (&info, MB_WM_TYPE_COMP_MGR_CLIENT, 0);
    }

  return type;
}

/*
 * This is a private method, hence static
 */
static MBWMCompMgrClient *
mb_wm_comp_mgr_xrender_client_new (MBWindowManagerClient * client)
{
  MBWMObject *c;

  c = mb_wm_object_new (MB_WM_TYPE_COMP_MGR_DEFAULT_CLIENT,
			MBWMObjectPropClient, client,
			NULL);

  return MB_WM_COMP_MGR_CLIENT (c);
}

static void
mb_wm_comp_mgr_xrender_add_damage (MBWMCompMgr * mgr, XserverRegion damage);

static XserverRegion
mb_wm_comp_mgr_xrender_client_extents (MBWMCompMgrClient *client);

static void
mb_wm_comp_mgr_xrender_client_hide_real (MBWMCompMgrClient * client)
{
  MBWMCompMgrDefaultClient * dclient  = MB_WM_COMP_MGR_DEFAULT_CLIENT (client);
  MBWindowManagerClient    * wm_client = client->wm_client;
  MBWindowManager          * wm        = client->wm;
  MBWMCompMgr              * mgr       = wm->comp_mgr;
  MBWindowManagerClient    * c;
  Bool                       is_modal  = mb_wm_client_is_modal (wm_client);

  if (is_modal && ((c = mb_wm_get_visible_main_client (wm)) != NULL))
    {
      XserverRegion extents;
      /* We need to make sure the any lowlighting on a 'parent'
       * modal for app gets cleared. This is kind of a sledgehammer
       * approach to it, but more suttle attempts oddly fail at times.
       *
       * FIXME: keep an eye on this for future revisions of composite
       *        - there may be a better way.
       */
      mb_wm_comp_mgr_xrender_client_repair_real (c->cm_client);
      extents = mb_wm_comp_mgr_xrender_client_extents (c->cm_client);
      mb_wm_comp_mgr_xrender_add_damage (mgr, extents);
    }

  if (dclient->damage)
    {
      XDamageDestroy (wm->xdpy, dclient->damage);
      dclient->damage = None;
    }

  if (dclient->extents)
    {
      mb_wm_comp_mgr_xrender_add_damage (mgr, dclient->extents);
      dclient->extents = None;
    }

  if (dclient->picture)
    {
      XRenderFreePicture (wm->xdpy, dclient->picture);
      dclient->picture = None;
    }
}

static void
mb_wm_comp_mgr_xrender_client_show_real (MBWMCompMgrClient * client)
{
  MBWMCompMgrDefaultClient * dclient  = MB_WM_COMP_MGR_DEFAULT_CLIENT (client);
  MBWindowManagerClient    * wm_client = client->wm_client;
  MBWindowManager          * wm        = client->wm;
  MBWMCompMgr              * mgr       = wm->comp_mgr;
  XserverRegion              region;
  XRenderPictureAttributes   pa;
  MBWMClientType             ctype = MB_WM_CLIENT_CLIENT_TYPE (wm_client);
  Bool                       is_modal;

  /*
   *  Destroying / Recreating the client pictures should hopefully save
   *  some memory in the server.
   */
  if (!dclient->picture)
    {
      pa.subwindow_mode = IncludeInferiors;

      dclient->picture =
	XRenderCreatePicture (wm->xdpy,
			      wm_client->xwin_frame ?
			      wm_client->xwin_frame :
			      wm_client->window->xwindow,
			      client->is_argb32 ?
			      XRenderFindStandardFormat (wm->xdpy,
							 PictStandardARGB32)

			      : XRenderFindVisualFormat (wm->xdpy,
						 wm_client->window->visual),
			      CPSubwindowMode,
			      &pa);
    }

  if (dclient->damage != None)
    XDamageDestroy (wm->xdpy, dclient->damage);

  dclient->damage = XDamageCreate (wm->xdpy,
				   wm_client->xwin_frame ?
				   wm_client->xwin_frame :
				   wm_client->window->xwindow,
				   XDamageReportNonEmpty);

  region = mb_wm_comp_mgr_xrender_client_extents (client);

  mb_wm_comp_mgr_xrender_add_damage (mgr, region);

  /*
   * If the wm client is modal we have to add its parent to the damage
   * in order for lowlighting to work
   */
  if (mb_wm_client_is_modal (wm_client))
    {
      MBWindowManagerClient * parent =
	mb_wm_client_get_transient_for (wm_client);

      if (parent && parent->cm_client)
	{
	  XserverRegion extents =
	    mb_wm_comp_mgr_xrender_client_extents (parent->cm_client);

	  mb_wm_comp_mgr_xrender_add_damage (mgr, extents);
	}
    }

  if (!dclient->extents)
    {
      dclient->extents = mb_wm_comp_mgr_xrender_client_extents (client);
    }
}


/*
 * The Manager itself
 */

typedef struct MBGaussianMap
{
  int	     size;
  double * data;
} MBGaussianMap;


struct MBWMCompMgrDefaultPrivate
{
  MBGaussianMap  * gaussian_map;

  Picture          shadow_n_pic;
  Picture          shadow_e_pic;
  Picture          shadow_s_pic;
  Picture          shadow_w_pic;

  Picture          shadow_ne_pic;
  Picture          shadow_nw_pic;
  Picture          shadow_se_pic;
  Picture          shadow_sw_pic;

  Picture          shadow_pic;

  int		   shadow_dx;
  int		   shadow_dy;
  int		   shadow_padding_width;
  int		   shadow_padding_height;
  int              shadow_style;
  unsigned int     shadow_color[4]; /* RGBA */

  Picture          trans_picture;
  Picture          black_picture;
  Picture          lowlight_picture;
  unsigned int     lowlight_params[4]; /* RGBA */

  Picture	   root_picture;
  Picture	   root_buffer;

  XserverRegion    all_damage;
  Bool             dialog_shade;
};

static void
mb_wm_comp_mgr_xrender_private_free (MBWMCompMgrDefault *mgr)
{
  MBWMCompMgrDefaultPrivate * priv = mgr->priv;
  Display                   * xdpy = MB_WM_COMP_MGR (mgr)->wm->xdpy;

  if (priv->gaussian_map)
    free (priv->gaussian_map);

  XRenderFreePicture (xdpy, priv->shadow_n_pic);
  XRenderFreePicture (xdpy, priv->shadow_e_pic);
  XRenderFreePicture (xdpy, priv->shadow_s_pic);
  XRenderFreePicture (xdpy, priv->shadow_w_pic);

  XRenderFreePicture (xdpy, priv->shadow_ne_pic);
  XRenderFreePicture (xdpy, priv->shadow_nw_pic);
  XRenderFreePicture (xdpy, priv->shadow_se_pic);
  XRenderFreePicture (xdpy, priv->shadow_sw_pic);

  XRenderFreePicture (xdpy, priv->shadow_pic);

  XRenderFreePicture (xdpy, priv->black_picture);
  XRenderFreePicture (xdpy, priv->lowlight_picture);
  XRenderFreePicture (xdpy, priv->trans_picture);

  if (priv->root_picture)
    XRenderFreePicture (xdpy, priv->root_picture);

  if (priv->root_buffer)
    XRenderFreePicture (xdpy, priv->root_buffer);

  if (priv->all_damage)
    XDamageDestroy (xdpy, priv->all_damage);

  free (priv);
}

static void
mb_wm_comp_mgr_xrender_register_client_real (MBWMCompMgr           * mgr,
					     MBWindowManagerClient * c)
{
  MBWMCompMgrDefaultPrivate * priv = MB_WM_COMP_MGR_DEFAULT (mgr)->priv;

  MBWM_NOTE (COMPOSITOR, "@@@@ registering client for %x @@@@\n",
	     c->window->xwindow);

  if (c->cm_client)
    return;

  c->cm_client = mb_wm_comp_mgr_xrender_client_new (c);
}

static void
mb_wm_comp_mgr_xrender_turn_on_real (MBWMCompMgr *mgr);

static void
mb_wm_comp_mgr_xrender_turn_off_real (MBWMCompMgr *mgr);

static void
mb_wm_comp_mgr_xrender_render_real (MBWMCompMgr *mgr);

static Bool
mb_wm_comp_mgr_xrender_handle_damage (XDamageNotifyEvent * de,
				      MBWMCompMgr        * mgr);

static void
mb_wm_comp_mgr_xrender_class_init (MBWMObjectClass *klass)
{
  MBWMCompMgrClass *cm_klass = MB_WM_COMP_MGR_CLASS (klass);

#if MBWM_WANT_DEBUG
  klass->klass_name = "MBWMCompMgrDefault";
#endif

  cm_klass->register_client   = mb_wm_comp_mgr_xrender_register_client_real;
  cm_klass->turn_on           = mb_wm_comp_mgr_xrender_turn_on_real;
  cm_klass->turn_off          = mb_wm_comp_mgr_xrender_turn_off_real;
  cm_klass->render            = mb_wm_comp_mgr_xrender_render_real;
  cm_klass->handle_damage     = mb_wm_comp_mgr_xrender_handle_damage;
}

static void
mb_wm_comp_mgr_xrender_init_pictures (MBWMCompMgr *mgr);

static int
mb_wm_comp_mgr_xrender_init (MBWMObject *obj, va_list vap)
{
  MBWMCompMgr                * mgr  = MB_WM_COMP_MGR (obj);
  MBWMCompMgrDefault         * dmgr = MB_WM_COMP_MGR_DEFAULT (obj);
  MBWindowManager            * wm   = mgr->wm;
  MBWMCompMgrDefaultPrivate  * priv;

  if (!wm)
    return 0;

  dmgr->priv = mb_wm_util_malloc0 (sizeof (MBWMCompMgrDefaultPrivate));
  priv       = dmgr->priv;

  priv->shadow_dx = SHADOW_OFFSET_X;
  priv->shadow_dy = SHADOW_OFFSET_Y;

  /* Not really used yet */
  priv->shadow_padding_width  = 0;
  priv->shadow_padding_height = 0;

  mgr->disabled = True;

  XCompositeRedirectSubwindows (wm->xdpy, wm->root_win->xwindow,
				CompositeRedirectManual);

  mb_wm_theme_get_shadow_color (wm->theme,
				&priv->shadow_color[0],
				&priv->shadow_color[1],
				&priv->shadow_color[2],
				&priv->shadow_color[3]);

  mb_wm_theme_get_lowlight_color (wm->theme,
				  &priv->lowlight_params[0],
				  &priv->lowlight_params[1],
				  &priv->lowlight_params[2],
				  &priv->lowlight_params[3]);

  priv->shadow_style = mb_wm_theme_get_shadow_type (wm->theme);

  mb_wm_comp_mgr_xrender_init_pictures (mgr);

  return 1;
}

static void
mb_wm_comp_mgr_xrender_destroy (MBWMObject * obj)
{
  MBWMCompMgr        * mgr  = MB_WM_COMP_MGR (obj);
  MBWMCompMgrDefault * dmgr = MB_WM_COMP_MGR_DEFAULT (obj);

  mb_wm_comp_mgr_turn_off (mgr);
  mb_wm_comp_mgr_xrender_private_free (dmgr);
}

int
mb_wm_comp_mgr_xrender_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (MBWMCompMgrDefaultClass),
	sizeof (MBWMCompMgrDefault),
	mb_wm_comp_mgr_xrender_init,
	mb_wm_comp_mgr_xrender_destroy,
	mb_wm_comp_mgr_xrender_class_init
      };

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_COMP_MGR, 0);
    }

  return type;
}

/* Shadow Generation */
static double
gaussian (double r, double x, double y)
{
  return ((1 / (sqrt (2 * M_PI * r))) *
	  exp ((- (x * x + y * y)) / (2 * r * r)));
}


static MBGaussianMap *
mb_wm_comp_mgr_xrender_make_gaussian_map (double r)
{
  MBGaussianMap  *c;
  int	          size = ((int) ceil ((r * 3)) + 1) & ~1;
  int	          center = size / 2;
  int	          x, y;
  double          t = 0.0;
  double          g;

  c = malloc (sizeof (MBGaussianMap) + size * size * sizeof (double));
  c->size = size;

  c->data = (double *) (c + 1);

  for (y = 0; y < size; y++)
    for (x = 0; x < size; x++)
      {
	g = gaussian (r, (double) (x - center), (double) (y - center));
	t += g;
	c->data[y * size + x] = g;
      }

  for (y = 0; y < size; y++)
    for (x = 0; x < size; x++)
      c->data[y*size + x] /= t;

  return c;
}

static unsigned char
mb_wm_comp_mgr_xrender_sum_gaussian (MBWMCompMgr * mgr, double opacity,
				     int x, int y, int width, int height)
{
  MBGaussianMap  * map = MB_WM_COMP_MGR_DEFAULT (mgr)->priv->gaussian_map;
  int	           fx, fy;
  double         * g_data;
  double         * g_line = map->data;
  int	           g_size = map->size;
  int	           center = g_size / 2;
  int	           fx_start, fx_end;
  int	           fy_start, fy_end;
  double           v;

  /*
   * Compute set of filter values which are "in range",
   * that's the set with:
   *	0 <= x + (fx-center) && x + (fx-center) < width &&
   *  0 <= y + (fy-center) && y + (fy-center) < height
   *
   *  0 <= x + (fx - center)	x + fx - center < width
   *  center - x <= fx	fx < width + center - x
   */

  fx_start = center - x;
  if (fx_start < 0)
    fx_start = 0;
  fx_end = width + center - x;
  if (fx_end > g_size)
    fx_end = g_size;

  fy_start = center - y;
  if (fy_start < 0)
    fy_start = 0;
  fy_end = height + center - y;
  if (fy_end > g_size)
    fy_end = g_size;

  g_line = g_line + fy_start * g_size + fx_start;

  v = 0;
  for (fy = fy_start; fy < fy_end; fy++)
    {
      g_data = g_line;
      g_line += g_size;

      for (fx = fx_start; fx < fx_end; fx++)
	v += *g_data++;
    }
  if (v > 1)
    v = 1;

  return ((unsigned int) (v * opacity * 255.0));
}

#define MAX_TILE_SZ 16 	/* make sure size/2 < MAX_TILE_SZ */
#define WIDTH  320
#define HEIGHT 320

static void
mb_wm_comp_mgr_xrender_shadow_setup_part (MBWMCompMgr  * mgr,
					  XImage      ** ximage,
					  Picture      * pic,
					  Pixmap       * pxm,
					  int            width,
					  int            height)
{
  MBWindowManager * wm = mgr->wm;

  *ximage = XCreateImage (wm->xdpy, DefaultVisual(wm->xdpy, wm->xscreen),
			  8, ZPixmap, 0, 0,
			  width, height, 8, width * sizeof (unsigned char));

  (*ximage)->data = malloc (width * height * sizeof (unsigned char));

  *pxm = XCreatePixmap (wm->xdpy, wm->root_win->xwindow,
			width, height, 8);

  *pic = XRenderCreatePicture (wm->xdpy, *pxm,
			       XRenderFindStandardFormat (wm->xdpy,
							  PictStandardA8),
			       0, 0);
}

static void
mb_wm_comp_mgr_xrender_shadow_finalise_part (MBWMCompMgr  * mgr,
					     XImage       * ximage,
					     Picture        pic,
					     Pixmap         pxm,
					     int            width,
					     int            height)
{
  MBWindowManager * wm = mgr->wm;

  GC gc = XCreateGC (wm->xdpy, pxm, 0, 0);
  XPutImage (wm->xdpy, pxm, gc, ximage, 0, 0, 0, 0, width, height);
  XDestroyImage (ximage);
  XFreeGC (wm->xdpy, gc);
  XFreePixmap (wm->xdpy, pxm);
}

static void
mb_wm_comp_mgr_xrender_shadow_setup (MBWMCompMgr * mgr)
{
  MBWindowManager            * wm = mgr->wm;
  MBWMCompMgrDefaultPrivate  * priv = MB_WM_COMP_MGR_DEFAULT (mgr)->priv;
  XImage	             * ximage;
  Pixmap                       pxm;
  unsigned char              * data;
  int		               size;
  int		               center;
  int		               x, y;
  unsigned char                d;
  int                          pwidth, pheight;
  double                       opacity = SHADOW_OPACITY;

  if (priv->shadow_style == MBWM_COMP_MGR_SHADOW_NONE)
    return;

  if (priv->shadow_style == MBWM_COMP_MGR_SHADOW_SIMPLE)
    {
      priv->shadow_padding_width  = 0;
      priv->shadow_padding_height = 0;
      return;
    }

  /* SHADOW_STYLE_GAUSSIAN */
  priv->gaussian_map =
    mb_wm_comp_mgr_xrender_make_gaussian_map (SHADOW_RADIUS);

  priv->shadow_padding_width  = priv->gaussian_map->size;
  priv->shadow_padding_height = priv->gaussian_map->size;

  size   = priv->gaussian_map->size;
  center = size / 2;

  /* Top & bottom */
  pwidth  = MAX_TILE_SZ;
  pheight = size/2;
  mb_wm_comp_mgr_xrender_shadow_setup_part (mgr,
					    &ximage, &priv->shadow_n_pic, &pxm,
					    pwidth, pheight);

  data = (unsigned char*)ximage->data;

  for (y = 0; y < pheight; y++)
    {
      d = mb_wm_comp_mgr_xrender_sum_gaussian (mgr, opacity,
					       center, y - center,
					       WIDTH, HEIGHT);
      for (x = 0; x < pwidth; x++)
	data[y * pwidth + x] = d;
    }

  mb_wm_comp_mgr_xrender_shadow_finalise_part (mgr, ximage, priv->shadow_n_pic,
					       pxm, pwidth, pheight);

  pwidth = MAX_TILE_SZ;
  pheight = MAX_TILE_SZ;

  mb_wm_comp_mgr_xrender_shadow_setup_part (mgr, &ximage, &priv->shadow_s_pic,
					    &pxm, pwidth, pheight);

  data = (unsigned char*)ximage->data;

  for (y = 0; y < pheight; y++)
    {
      d = mb_wm_comp_mgr_xrender_sum_gaussian (mgr, opacity,
					       center, y - center,
					       WIDTH, HEIGHT);
      for (x = 0; x < pwidth; x++)
	data[(pheight - y - 1) * pwidth + x] = d;
    }

  mb_wm_comp_mgr_xrender_shadow_finalise_part (mgr, ximage, priv->shadow_s_pic,
					       pxm, pwidth, pheight);

  /* Sides */
  pwidth = MAX_TILE_SZ;
  pheight = MAX_TILE_SZ;
  mb_wm_comp_mgr_xrender_shadow_setup_part (mgr, &ximage, &priv->shadow_w_pic,
					    &pxm, pwidth, pheight);

  data = (unsigned char*)ximage->data;

  for (x = 0; x < pwidth; x++)
    {
      d = mb_wm_comp_mgr_xrender_sum_gaussian (mgr, opacity,
					       x - center, center,
					       WIDTH, HEIGHT);
      for (y = 0; y < pheight; y++)
	data[y * pwidth + (pwidth - x - 1)] = d;
    }

  mb_wm_comp_mgr_xrender_shadow_finalise_part (mgr, ximage, priv->shadow_w_pic,
					       pxm, pwidth, pheight);

  mb_wm_comp_mgr_xrender_shadow_setup_part (mgr, &ximage, &priv->shadow_e_pic,
					    &pxm, pwidth, pheight);

  data = (unsigned char*)ximage->data;

  for (x = 0; x < pwidth; x++)
    {
      d = mb_wm_comp_mgr_xrender_sum_gaussian (mgr, opacity,
					       x - center, center,
					       WIDTH, HEIGHT);
      for (y = 0; y < pheight; y++)
	data[y * pwidth + x] = d;
    }

  mb_wm_comp_mgr_xrender_shadow_finalise_part (mgr, ximage, priv->shadow_e_pic,
					       pxm, pwidth, pheight);

  /* Corners */
  pwidth = MAX_TILE_SZ;
  pheight = MAX_TILE_SZ;
  mb_wm_comp_mgr_xrender_shadow_setup_part (mgr, &ximage, &priv->shadow_nw_pic,
					    &pxm, pwidth, pheight);

  data = (unsigned char*)ximage->data;

  for (x = 0; x < pwidth; x++)
    for (y = 0; y < pheight; y++)
      {
	d = mb_wm_comp_mgr_xrender_sum_gaussian (mgr, opacity,
						 x-center, y-center,
						 WIDTH, HEIGHT);

	data[y * pwidth + x] = d;
      }

  mb_wm_comp_mgr_xrender_shadow_finalise_part (mgr, ximage,
					       priv->shadow_nw_pic,
					       pxm, pwidth, pheight);

  mb_wm_comp_mgr_xrender_shadow_setup_part (mgr, &ximage, &priv->shadow_sw_pic,
					    &pxm, pwidth, pheight);

  data = (unsigned char*)ximage->data;

  for (x = 0; x < pwidth; x++)
    for (y = 0; y < pheight; y++)
      {
	d = mb_wm_comp_mgr_xrender_sum_gaussian (mgr, opacity,
						 x-center, y-center,
						 WIDTH, HEIGHT);

	data[(pheight - y - 1) * pwidth + x] = d;
      }

  mb_wm_comp_mgr_xrender_shadow_finalise_part (mgr, ximage,
					       priv->shadow_sw_pic,
					       pxm, pwidth, pheight);

  mb_wm_comp_mgr_xrender_shadow_setup_part (mgr, &ximage, &priv->shadow_se_pic,
					    &pxm, pwidth, pheight);

  data = (unsigned char*)ximage->data;

  for (x = 0; x < pwidth; x++)
    for (y = 0; y < pheight; y++)
      {
	d = mb_wm_comp_mgr_xrender_sum_gaussian (mgr, opacity,
						 x-center, y-center,
						 WIDTH, HEIGHT);

	data[(pheight - y - 1) * pwidth + (pwidth - x -1)] = d;
      }

  mb_wm_comp_mgr_xrender_shadow_finalise_part (mgr, ximage,
					       priv->shadow_se_pic,
					       pxm, pwidth, pheight);

  mb_wm_comp_mgr_xrender_shadow_setup_part(mgr, &ximage, &priv->shadow_ne_pic,
					   &pxm, pwidth, pheight);

  data = (unsigned char*)ximage->data;

  for (x = 0; x < pwidth; x++)
    for (y = 0; y < pheight; y++)
      {
	d = mb_wm_comp_mgr_xrender_sum_gaussian (mgr, opacity,
					 x-center, y-center, WIDTH, HEIGHT);

	data[y * pwidth + (pwidth - x -1)] = d;
      }

  mb_wm_comp_mgr_xrender_shadow_finalise_part (mgr, ximage,
					       priv->shadow_ne_pic,
					       pxm, pwidth, pheight);

  /* Finally center */
  pwidth = MAX_TILE_SZ;
  pheight = MAX_TILE_SZ;
  mb_wm_comp_mgr_xrender_shadow_setup_part (mgr, &ximage, &priv->shadow_pic,
					    &pxm, pwidth, pheight);

  data = (unsigned char*)ximage->data;

  d = mb_wm_comp_mgr_xrender_sum_gaussian (mgr, opacity,
					   center, center, WIDTH, HEIGHT);

  for (x = 0; x < pwidth; x++)
    for (y = 0; y < pheight; y++)
      data[y * pwidth + x] = d;

  mb_wm_comp_mgr_xrender_shadow_finalise_part (mgr, ximage, priv->shadow_pic,
					       pxm, pwidth, pheight);

}

static Picture
mb_wm_comp_mgr_xrender_shadow_gaussian_make_picture (MBWMCompMgr * mgr,
						     int width, int height)
{
  MBWindowManager            * wm   = mgr->wm;
  MBWMCompMgrDefaultPrivate  * priv = MB_WM_COMP_MGR_DEFAULT (mgr)->priv;
  Picture                      pic;
  Pixmap                       pxm;
  int                          pwidth, pheight, x, y, dw, dh;

  pxm = XCreatePixmap (wm->xdpy, wm->root_win->xwindow, width, height, 8);
  pic = XRenderCreatePicture (wm->xdpy, pxm,
			      XRenderFindStandardFormat (wm->xdpy,
							 PictStandardA8),
			      0,0);

  pwidth = MAX_TILE_SZ;
  pheight = MAX_TILE_SZ;

  for (x=0; x < width; x += pwidth)
    for (y=0; y < height; y += pheight)
      {
	if ( (y + pheight) > height )
	  dh = pheight - ((y + pheight)-height);
	else
	  dh = pheight;

	if ( (x + pwidth) > width )
	  dw = pwidth - ((x + pwidth)-width);
	else
	  dw = pwidth;

	XRenderComposite (wm->xdpy, PictOpSrc,
			  priv->shadow_pic, None, pic,
			  0, 0, 0, 0, x, y, dw, dh);
      }

  /* Top & bottom */
  if ( width > (MAX_TILE_SZ*2) )
    {
      pwidth = MAX_TILE_SZ; pheight = MAX_TILE_SZ;

      for (x=0; x < width; x += pwidth )
	{
	  if ( (x + pwidth) > width )
	    dw = pwidth - ((x + pwidth)-width);
	  else
	    dw = pwidth;

	  XRenderComposite (wm->xdpy, PictOpSrc,
			    priv->shadow_n_pic, None, pic,
			    0, 0, 0, 0, x, 0, dw, pheight);
	  XRenderComposite (wm->xdpy, PictOpSrc,
			    priv->shadow_s_pic, None, pic,
			    0, 0, 0, 0, x, height - pheight, dw, pheight);
	}
    }

  /* Sides */
  if ( height > (MAX_TILE_SZ*2) )
    {
      pwidth = MAX_TILE_SZ; pheight = MAX_TILE_SZ;

      for (y=0; y < height; y += pheight)
	{
	  if ( (y + pheight) > height )
	    dh = pheight - ((y + pheight)-height);
	  else
	    dh = pheight;

	  XRenderComposite (wm->xdpy, PictOpSrc /* PictOpIn */,
			    priv->shadow_e_pic, None, pic,
			    0, 0, 0, 0, 0, y, pwidth, dh);
	  XRenderComposite (wm->xdpy, PictOpSrc /* PictOpIn */,
			    priv->shadow_w_pic, None, pic,
			    0, 0, 0, 0, width - pwidth, y, pwidth, dh);
	}
    }

  /* Corners */
  pwidth = MAX_TILE_SZ; pheight = MAX_TILE_SZ;

  XRenderComposite (wm->xdpy, PictOpSrc, priv->shadow_nw_pic, None, pic,
		    0, 0, 0, 0, 0, 0, pwidth, pheight);

  XRenderComposite (wm->xdpy, PictOpSrc, priv->shadow_ne_pic, None, pic,
		    0, 0, 0, 0, width - pwidth, 0, pwidth, pheight);

  XRenderComposite (wm->xdpy, PictOpSrc, priv->shadow_sw_pic, None, pic,
		    0, 0, 0, 0, 0, height - pheight, pwidth, pheight);

  XRenderComposite (wm->xdpy, PictOpSrc, priv->shadow_se_pic, None, pic,
		    0, 0, 0, 0, width - pwidth, height - pheight,
		    pwidth, pheight);

  XFreePixmap (wm->xdpy, pxm);
  return pic;
}

static XserverRegion
mb_wm_comp_mgr_xrender_client_extents (MBWMCompMgrClient *client)
{
  MBWindowManagerClient     *wm_client = client->wm_client;
  MBWindowManager           *wm = client->wm;
  MBWMCompMgr               *mgr = wm->comp_mgr;
  MBWMCompMgrDefaultPrivate *priv = MB_WM_COMP_MGR_DEFAULT (mgr)->priv;
  MBGeometry                 geom;
  XRectangle	             r;
  MBWMClientType             ctype = MB_WM_CLIENT_CLIENT_TYPE (wm_client);
  XserverRegion              extents;

  mb_wm_client_get_coverage (wm_client, &geom);

  r.x      = geom.x;
  r.y      = geom.y;
  r.width  = geom.width;
  r.height = geom.height;

  if (priv->shadow_style)
    {
      if (ctype == MBWMClientTypeDialog   ||
	  ctype == MBWMClientTypeMenu     ||
	  ctype == MBWMClientTypeOverride)
	{
	  if (priv->shadow_style == MBWM_COMP_MGR_SHADOW_SIMPLE)
	    {
	      r.width  += priv->shadow_dx;
	      r.height += priv->shadow_dy;
	    }
	  else
	    {
	      r.x      += priv->shadow_dx;
	      r.y      += priv->shadow_dy;
	      r.width  += priv->shadow_padding_width;
	      r.height += priv->shadow_padding_height;
	    }
	}
    }

  extents = XFixesCreateRegion (wm->xdpy, &r, 1);

  return extents;
}

static XserverRegion
mb_wm_comp_mgr_xrender_client_border_size (MBWMCompMgrClient  * client,
					   int x, int y)
{
  MBWindowManagerClient * wm_client = client->wm_client;
  MBWindowManager       * wm        = client->wm;
  XserverRegion           border;

  border = XFixesCreateRegionFromWindow (wm->xdpy,
					 wm_client->xwin_frame ?
					 wm_client->xwin_frame :
					 wm_client->window->xwindow,
					 WindowRegionBounding);
  /* translate this */
  XFixesTranslateRegion (wm->xdpy, border, x, y);
  return border;
}

static XserverRegion
mb_wm_comp_mgr_xrender_client_window_region (MBWMCompMgrClient  *client,
					     Window xwin, int x, int y)
{
  MBWindowManagerClient * wm_client = client->wm_client;
  MBWindowManager       * wm        = client->wm;
  XserverRegion           region;

  region =
    XFixesCreateRegionFromWindow (wm->xdpy, xwin, WindowRegionBounding);

  /* translate this */
  XFixesTranslateRegion (wm->xdpy, region, x, y);

  return region;
}

static Visual*
mb_wm_comp_mgr_xrender_get_argb32_visual (MBWMCompMgr * mgr)
{
  MBWindowManager     * wm = mgr->wm;
  XVisualInfo	      * xvi;
  XVisualInfo	        template;
  int			nvi;
  int			i;
  XRenderPictFormat   * format;
  Visual	      * visual = NULL;

  template.screen = wm->xscreen;
  template.depth  = 32;
  template.class  = TrueColor;

  if ((xvi = XGetVisualInfo (wm->xdpy,
			     VisualScreenMask|VisualDepthMask|VisualClassMask,
			     &template,
			     &nvi)) == NULL)
    return NULL;

  for (i = 0; i < nvi; i++)
    {
      format = XRenderFindVisualFormat (wm->xdpy, xvi[i].visual);
      if (format->type == PictTypeDirect && format->direct.alphaMask)
	{
	  visual = xvi[i].visual;
	  break;
	}
    }

  XFree (xvi);
  return visual;
}

static void
mb_wm_comp_mgr_xrender_init_pictures (MBWMCompMgr *mgr)
{
  MBWindowManager             * wm;
  Window                        rwin;
  MBWMCompMgrDefaultPrivate   * priv;
  Pixmap                        transPixmap, blackPixmap, lowlightPixmap,
                                redPixmap;
  XRenderPictureAttributes	pa;
  XRenderColor                  c;
  int                           i;

  if (!mgr)
    return;

  wm = mgr->wm;
  rwin = wm->root_win->xwindow;
  priv = MB_WM_COMP_MGR_DEFAULT (mgr)->priv;

  {
    Picture  pics_to_free[] = { priv->trans_picture,
				priv->black_picture,
				priv->lowlight_picture,
				priv->shadow_n_pic,
				priv->shadow_e_pic,
				priv->shadow_s_pic,
				priv->shadow_w_pic,
				priv->shadow_ne_pic,
				priv->shadow_nw_pic,
				priv->shadow_se_pic,
				priv->shadow_sw_pic,
				priv->shadow_pic };

    for (i=0; i < (sizeof(pics_to_free)/sizeof(Picture)); i++)
      if (pics_to_free[i] != None)
	XRenderFreePicture (wm->xdpy, pics_to_free[i]);
  }

  if (priv->shadow_style == MBWM_COMP_MGR_SHADOW_GAUSSIAN)
    mb_wm_comp_mgr_xrender_shadow_setup (mgr);

  pa.subwindow_mode = IncludeInferiors;
  pa.repeat         = True;

  transPixmap = XCreatePixmap (wm->xdpy, rwin, 1, 1, 8);

  priv->trans_picture
    = XRenderCreatePicture (wm->xdpy, transPixmap,
			    XRenderFindStandardFormat (wm->xdpy,
						       PictStandardA8),
			    CPRepeat,
			    &pa);

  c.red = c.green = c.blue = 0;
  c.alpha = 0xddff;

  XRenderFillRectangle (wm->xdpy, PictOpSrc, priv->trans_picture,
			&c, 0, 0, 1, 1);

  /* black pixmap used for shadows */

  blackPixmap = XCreatePixmap (wm->xdpy, rwin, 1, 1, 32);

  priv->black_picture
    = XRenderCreatePicture (wm->xdpy, blackPixmap,
			    XRenderFindStandardFormat (wm->xdpy,
						       PictStandardARGB32),
			    CPRepeat,
			    &pa);

  c.red   = priv->shadow_color[0];
  c.green = priv->shadow_color[1];
  c.blue  = priv->shadow_color[2];

  if (priv->shadow_style == MBWM_COMP_MGR_SHADOW_GAUSSIAN)
    c.alpha = 0xffff;
  else
    c.alpha = priv->shadow_color[3];

  XRenderFillRectangle (wm->xdpy, PictOpSrc, priv->black_picture,
			&c, 0, 0, 1, 1);

  /* Used for lowlights */
  lowlightPixmap = XCreatePixmap (wm->xdpy, rwin, 1, 1, 32);
  priv->lowlight_picture
    = XRenderCreatePicture (wm->xdpy, lowlightPixmap,
			    XRenderFindStandardFormat (wm->xdpy,
						       PictStandardARGB32),
			    CPRepeat,
			    &pa);

  c.red   = priv->lowlight_params[0];
  c.green = priv->lowlight_params[1];
  c.blue  = priv->lowlight_params[2];
  c.alpha = priv->lowlight_params[3];

  XRenderFillRectangle (wm->xdpy, PictOpSrc, priv->lowlight_picture,
                        &c, 0, 0, 1, 1);


  pa.repeat = False;

  priv->root_picture
    = XRenderCreatePicture (wm->xdpy, rwin,
			    XRenderFindVisualFormat (wm->xdpy,
						     DefaultVisual (wm->xdpy,
								wm->xscreen)),
			    CPSubwindowMode,
			    &pa);

  priv->all_damage = None;
}

/* Shuts the compositing down */
static void
mb_wm_comp_mgr_xrender_turn_off_real (MBWMCompMgr *mgr)
{
  MBWindowManager            * wm;
  Window                       rwin;
  MBWMCompMgrDefaultPrivate  * priv;
  MBWMList                   * l;

  if (!mgr)
    return;

  priv = MB_WM_COMP_MGR_DEFAULT (mgr)->priv;

  if (mgr->disabled)
    return;

  wm   = mgr->wm;
  rwin = wm->root_win->xwindow;

  /*
   *  really shut down the composite manager.
   */
  XCompositeUnredirectSubwindows (wm->xdpy, rwin, CompositeRedirectManual);

  if (priv->root_picture)
    {
      XRenderFreePicture (wm->xdpy, priv->root_picture);
      priv->root_picture = None;
    }

  if (priv->root_buffer)
    {
      XRenderFreePicture (wm->xdpy, priv->root_buffer);
      priv->root_buffer  = None;
    }

  if (priv->all_damage)
    {
      XDamageDestroy (wm->xdpy, priv->all_damage);
      priv->all_damage = None;
    }

  /* Free up any client composite resources */
  l = wm->clients;

  while (l)
    {
      MBWindowManagerClient * wmc = l->data;
      MBWMCompMgrClient     * c   = wmc->cm_client;

      if (c)
	{
	  mb_wm_object_unref (MB_WM_OBJECT (c));
	  wmc->cm_client = NULL;
	}

      l = l->next;
    }

  mgr->disabled = True;
}

static void
mb_wm_comp_mgr_xrender_render_region (MBWMCompMgr *mgr, XserverRegion region);

static void
mb_wm_comp_mgr_xrender_turn_on_real (MBWMCompMgr *mgr)
{
  MBWindowManager            * wm;
  Window                       rwin;
  MBWMCompMgrDefaultPrivate  * priv;
  MBWMList                   * l;

  if (!mgr)
    return;

  priv = MB_WM_COMP_MGR_DEFAULT (mgr)->priv;

  if (!mgr->disabled)
    return;

  wm   = mgr->wm;
  rwin = wm->root_win->xwindow;

  XCompositeRedirectSubwindows (wm->xdpy, wm->root_win->xwindow,
				CompositeRedirectManual);

  mb_wm_comp_mgr_xrender_init_pictures (mgr);

  XSync (wm->xdpy, False);

  mgr->disabled = False;

  if (!mb_wm_stack_empty (wm))
    {
      MBWindowManagerClient * c;

      mb_wm_stack_enumerate (wm, c)
	{
	  mb_wm_comp_mgr_xrender_register_client_real (mgr, c);
	  mb_wm_comp_mgr_xrender_client_show_real (c->cm_client);
	}

      mb_wm_comp_mgr_xrender_render_region (mgr, None);
    }
}

static int
mb_wm_comp_mgr_xrender_client_get_translucency (MBWMCompMgrClient *client)
{
  MBWindowManagerClient * wm_client = client->wm_client;

  return wm_client->window->translucency;
}

static void
mb_wm_comp_mgr_xrender_add_damage (MBWMCompMgr * mgr, XserverRegion damage)
{
  MBWMCompMgrDefaultPrivate * priv = MB_WM_COMP_MGR_DEFAULT (mgr)->priv;
  MBWindowManager    * wm = mgr->wm;

  if (priv->all_damage)
    {
      XFixesUnionRegion (wm->xdpy, priv->all_damage, priv->all_damage,
			 damage);

      XFixesDestroyRegion (wm->xdpy, damage);
    }
    else
      priv->all_damage = damage;

  mb_wm_display_sync_queue (wm, MBWMSyncVisibility);
}

static void
mb_wm_comp_mgr_xrender_client_repair_real (MBWMCompMgrClient * client)
{
  MBWindowManagerClient * wm_client = client->wm_client;
  MBWindowManager       * wm        = client->wm;
  MBWMCompMgr           * mgr       = wm->comp_mgr;
  XserverRegion           parts;
  MBGeometry              geom;

  parts = XFixesCreateRegion (wm->xdpy, 0, 0);

  /* translate region */
  XDamageSubtract (wm->xdpy, MB_WM_COMP_MGR_DEFAULT_CLIENT (client)->damage,
		   None, parts);

  mb_wm_client_get_coverage (wm_client, &geom);

  XFixesTranslateRegion (wm->xdpy, parts, geom.x, geom.y);
  mb_wm_comp_mgr_xrender_add_damage (mgr, parts);
}

static void
mb_wm_comp_mgr_xrender_client_configure_real (MBWMCompMgrClient * client)
{
  MBWMCompMgrDefaultClient * dclient  = MB_WM_COMP_MGR_DEFAULT_CLIENT (client);
  MBWindowManagerClient    * wm_client = client->wm_client;
  MBWindowManager          * wm        = client->wm;
  MBWMCompMgr              * mgr       = wm->comp_mgr;
  XserverRegion              damage    = None;
  XserverRegion              extents;

  extents = mb_wm_comp_mgr_xrender_client_extents (client);

  if (dclient->picture)
    {
      XRenderFreePicture (wm->xdpy, dclient->picture);
      dclient->picture = None;
    }

  damage = XFixesCreateRegion (wm->xdpy, 0, 0);

  if (dclient->extents)
    {
      XFixesCopyRegion (wm->xdpy, damage, dclient->extents);
      XFixesDestroyRegion (wm->xdpy, dclient->extents);
    }

  XFixesUnionRegion (wm->xdpy, damage, damage, extents);

  dclient->extents = extents;

  mb_wm_comp_mgr_xrender_add_damage (mgr, damage);
}

static Bool
mb_wm_comp_mgr_xrender_handle_damage (XDamageNotifyEvent * de,
				      MBWMCompMgr        * mgr)
{
  MBWMCompMgrDefaultPrivate * priv = MB_WM_COMP_MGR_DEFAULT (mgr)->priv;
  MBWindowManager           * wm   = mgr->wm;
  MBWindowManagerClient     * c;

  c = mb_wm_managed_client_from_frame (wm, de->drawable);

  if (c && c->cm_client)
    {
      MBWM_NOTE (COMPOSITOR,
		 "Reparing window %x, a %d,%d;%dx%d, g %d,%d;%dx%d\n",
		 de->drawable,
		 de->area.x,
		 de->area.y,
		 de->area.width,
		 de->area.height,
		 de->geometry.x,
		 de->geometry.y,
		 de->geometry.width,
		 de->geometry.height);

      mb_wm_comp_mgr_xrender_client_repair_real (c->cm_client);
    }
  else
    {
      MBWM_NOTE (COMPOSITOR, "Failed to find client for window %x\n",
		 de->drawable);
    }

  return False;
}

static void
_render_a_client (MBWMCompMgrClient * client,
		  XserverRegion       region,
		  int                 lowlight_type) /*0 none, 1 app, 2 full*/
{
  MBWMCompMgrDefaultClient * dclient  = MB_WM_COMP_MGR_DEFAULT_CLIENT (client);
  MBWindowManagerClient     * wm_client = client->wm_client;
  MBWindowManager           * wm        = client->wm;
  MBWMCompMgr               * mgr       = wm->comp_mgr;
  MBWMCompMgrDefaultPrivate * priv      = MB_WM_COMP_MGR_DEFAULT (mgr)->priv;
  MBWMClientType              ctype     = MB_WM_CLIENT_CLIENT_TYPE (wm_client);
  MBGeometry                  geom;

  if (!dclient->picture)
    return;

  mb_wm_client_get_coverage (wm_client, &geom);

  /* Translucency only done for dialogs and overides */
  if ( !client->is_argb32 &&
       (ctype == MBWMClientTypeApp     ||
	ctype == MBWMClientTypeDesktop ||
	ctype == MBWMClientTypeInput   ||
	ctype == MBWMClientTypePanel   ||
	mb_wm_comp_mgr_xrender_client_get_translucency (client) == -1))
    {
      XserverRegion winborder;

      winborder = mb_wm_comp_mgr_xrender_client_border_size (client,
							     geom.x, geom.y);

      XFixesSetPictureClipRegion (wm->xdpy, priv->root_buffer, 0, 0, region);

      XFixesSubtractRegion (wm->xdpy, region, region, winborder);

      XRenderComposite (wm->xdpy, PictOpSrc,
			dclient->picture,
			None, priv->root_buffer,
			0, 0, 0, 0,
			geom.x, geom.y, geom.width, geom.height);

      XFixesDestroyRegion (wm->xdpy, winborder);
    }
  else if (client->is_argb32 ||
	   mb_wm_comp_mgr_xrender_client_get_translucency (client) != -1)
    {
      /*
       * If the client is translucent, paint the decors only (solid).
       */
      MBWMList * l = wm_client->decor;

      while (l)
	{
	  MBWMDecor     * d = l->data;
	  MBGeometry    * g = & d->geom;
	  XserverRegion   r;

	  r = mb_wm_comp_mgr_xrender_client_window_region (client, d->xwin,
							   g->x, g->y);

	  XFixesSetPictureClipRegion (wm->xdpy, priv->root_buffer, 0, 0, r);
	  XFixesSubtractRegion (wm->xdpy, region, region, r);

	  XRenderComposite (wm->xdpy, PictOpSrc,
			    dclient->picture,
			    None, priv->root_buffer,
			    0, 0, 0, 0,
			    geom.x + g->x, geom.y + g->y,
			    g->width, g->height);

	  XFixesDestroyRegion (wm->xdpy, r);

	  l = l->next;
	}
    }


  /* Render lowlight dialog modal for app */
  if (lowlight_type == 1 &&
      (ctype & (MBWMClientTypeApp | MBWMClientTypeDesktop)))
    {
      int title_offset = 0;

      if (ctype == MBWMClientTypeApp)
	title_offset = mb_wm_client_title_height (wm_client);

      XRenderComposite (wm->xdpy, PictOpOver, priv->lowlight_picture, None,
			priv->root_buffer,
			0, 0, 0, 0, geom.x, geom.y + title_offset,
			geom.width, geom.height - title_offset);
    }
  else if (lowlight_type == 2 /* && client->win_modal_blocker == None */)
    {
      /* Render lowlight dialog modal for root - e.g lowlight everything */
      XRenderComposite (wm->xdpy, PictOpOver, priv->lowlight_picture, None,
			priv->root_buffer,
			0, 0, 0, 0, geom.x, geom.y,
			geom.width, geom.height);
    }

  if (dclient->border_clip)
    {
      XFixesDestroyRegion (wm->xdpy, dclient->border_clip);
      dclient->border_clip = None;
    }

  dclient->border_clip = XFixesCreateRegion (wm->xdpy, 0, 0);
  XFixesCopyRegion (wm->xdpy, dclient->border_clip, region);
}

static void
mb_wm_comp_mgr_xrender_render_real (MBWMCompMgr *mgr)
{
  MBWMCompMgrDefaultPrivate * priv = MB_WM_COMP_MGR_DEFAULT (mgr)->priv;

  mb_wm_comp_mgr_xrender_render_region (mgr, priv->all_damage);

  if (priv->all_damage)
    {
      XDamageDestroy (mgr->wm->xdpy, priv->all_damage);
      priv->all_damage = None;
    }
}

static void
mb_wm_comp_mgr_xrender_render_region (MBWMCompMgr *mgr, XserverRegion region)
{
  MBWindowManager            * wm   = mgr->wm;
  MBWMCompMgrDefaultPrivate  * priv = MB_WM_COMP_MGR_DEFAULT (mgr)->priv;
  MBWindowManagerClient      * wmc_top, * wmc_temp, * wmc_solid = NULL;
  int                          lowlight = 0;
  int                          destroy_region = 0;
  Bool                         done;
  Bool                         top_translucent = False;

  if (mgr->disabled)
    return;

  if (!region)
    {
      /*
       * Fullscreen render
       */
      XRectangle  r;

      r.x      = 0;
      r.y      = 0;
      r.width  = wm->xdpy_width;
      r.height = wm->xdpy_height;

      region = XFixesCreateRegion (wm->xdpy, &r, 1);
      destroy_region = 1;
    }

  wmc_top = mb_wm_get_visible_main_client (wm);

  if (wmc_top)
    top_translucent =
      (mb_wm_comp_mgr_xrender_client_get_translucency(wmc_top->cm_client) == -1);

  if (!priv->root_buffer)
    {
      Pixmap rootPixmap =
	XCreatePixmap (wm->xdpy, wm->root_win->xwindow,
		       wm->xdpy_width, wm->xdpy_height,
		       DefaultDepth (wm->xdpy, wm->xscreen));

      priv->root_buffer =
	XRenderCreatePicture (wm->xdpy, rootPixmap,
			      XRenderFindVisualFormat (wm->xdpy,
						DefaultVisual (wm->xdpy,
							       wm->xscreen)),
			      0, 0);

      XFreePixmap (wm->xdpy, rootPixmap);
    }

  XFixesSetPictureClipRegion (wm->xdpy, priv->root_picture, 0, 0, region);
  XFixesSetPictureClipRegion (wm->xdpy, priv->root_buffer, 0, 0, region);

  XRenderComposite (wm->xdpy, PictOpSrc, priv->black_picture,
		    None, priv->root_buffer, 0, 0, 0, 0, 0, 0,
		    wm->xdpy_width, wm->xdpy_height);

  /*
   * Check initially to see what kind of lowlight todo ( if any )
   */
  mb_wm_stack_enumerate_reverse (wm, wmc_temp)
    {
      MBWMClientType type = MB_WM_CLIENT_CLIENT_TYPE (wmc_temp);
      Bool is_modal = mb_wm_client_is_modal (wmc_temp);

      if (type == MBWMClientTypeDialog && is_modal)
	{
	  MBWMModality modality = mb_wm_get_modality_type (wm);
	  switch (modality)
	    {
	    case MBWMModalityNormal:
	    default:
	      lowlight = 1;
	      break;
	    case MBWMModalitySystem:
	      lowlight = 2;
	      break;
	    case MBWMModalityNone:
	      lowlight = 0;
	      break;
	    }
	}

      if (wmc_temp == wmc_top)
	break;
    }

  /* Render top -> bottom */
  done = False;
  mb_wm_stack_enumerate_reverse (wm, wmc_temp)
    {
      _render_a_client(wmc_temp->cm_client, region, lowlight);

      /*
       * Render clients until we reach first client on/below the top
       * which is not translucent and is either and application or desktop
       * (to have adequate coverage).
       */
      if (wmc_temp == wmc_top)
	{
	  done = True;
	}

      if (done &&
	  (MB_WM_CLIENT_CLIENT_TYPE (wmc_temp) &
	   (MBWMClientTypeApp | MBWMClientTypeDesktop)) &&
	 !wmc_temp->cm_client->is_argb32 &&
	  mb_wm_comp_mgr_xrender_client_get_translucency (wmc_temp->cm_client)
	  == -1)
	{
	  wmc_solid = wmc_temp;
	  break;
	}
    }

  if (!wmc_top)
    {
      /* Render block of boring black in case of no top app or desktop */
      XFixesSetPictureClipRegion (wm->xdpy, priv->root_buffer, 0, 0, region);

      XRenderComposite (wm->xdpy, PictOpSrc, priv->black_picture,
			None, priv->root_buffer, 0, 0, 0, 0, 0, 0,
			wm->xdpy_width, wm->xdpy_height);

      XFixesSetPictureClipRegion (wm->xdpy, priv->root_buffer, 0, 0, None);

      wmc_top = wm->stack_bottom;
    }


  XFixesSetPictureClipRegion (wm->xdpy, priv->root_buffer, 0, 0, None);

  /*
   * Now render shadows and any translucent clients but bottom -> top this
   * time
   *
   * We start from the first solid client on the stack, so that any
   * translucent windows on the top of the stack get correctly rendered.
   */
  for (wmc_temp = wmc_solid ? wmc_solid : wmc_top;
       wmc_temp; wmc_temp=wmc_temp->stacked_above)
    {
      MBWMClientType             type = MB_WM_CLIENT_CLIENT_TYPE (wmc_temp);
      MBWMCompMgrClient        * c    = wmc_temp->cm_client;
      MBWMCompMgrDefaultClient * dc   = MB_WM_COMP_MGR_DEFAULT_CLIENT (c);
      Bool                       is_translucent;

      if (!dc || !dc->picture)
	continue;

      /*
       * We have to process all dialogs and, if the top client is translucent,
       * any translucent windows as well.
       */
      is_translucent = (c->is_argb32 ||
			mb_wm_comp_mgr_xrender_client_get_translucency (c)
			!= -1);

      if (mb_wm_client_is_mapped (wmc_temp) &&
	  (type == MBWMClientTypeDialog ||
	   type == MBWMClientTypeMenu   ||
	   type == MBWMClientTypeOverride ||
	   (top_translucent && is_translucent)))
	{
	  if (priv->shadow_style)
	    {
	      Picture    shadow_pic;
	      MBGeometry geom;

	      mb_wm_client_get_coverage (wmc_temp, &geom);

	      if (priv->shadow_style == MBWM_COMP_MGR_SHADOW_SIMPLE)
		{
		  XserverRegion shadow_region;

		  /* Grab 'shape' region of window */
		  shadow_region =
		    mb_wm_comp_mgr_xrender_client_border_size (c,
							       geom.x, geom.y);

		  /* Offset it. */
		  XFixesTranslateRegion (wm->xdpy, shadow_region,
					 priv->shadow_dx,
					 priv->shadow_dy);

		  /* Intersect it, so only border remains */
		  XFixesIntersectRegion (wm->xdpy, shadow_region,
					 dc->border_clip,
					 shadow_region );

		  XFixesSetPictureClipRegion (wm->xdpy, priv->root_buffer,
					      0, 0, shadow_region);

		  /* now paint them */
		  if (wmc_temp->cm_client->is_argb32)
		    {
		      XRenderComposite (wm->xdpy, PictOpOver,
					priv->black_picture,
					dc->picture,
 					priv->root_buffer,
					0, 0, 0, 0,
					geom.x + priv->shadow_dx,
					geom.y + priv->shadow_dy,
					geom.width +
					priv->shadow_padding_width,
					geom.height +
					priv->shadow_padding_height);
		    }
		  else
		    {
		      XRenderComposite (wm->xdpy, PictOpOver,
					priv->black_picture,
					None,
					priv->root_buffer,
					0, 0, 0, 0,
					geom.x + priv->shadow_dx,
					geom.y + priv->shadow_dy,
					geom.width +
					priv->shadow_padding_width,
					geom.height +
					priv->shadow_padding_height);
		    }

		  /* Paint any translucent window contents, but no the
		   * decors.
		   */
		  if (is_translucent)
		    {
		      MBGeometry * win_geom = & wmc_temp->window->geometry;

		      XFixesDestroyRegion (wm->xdpy, shadow_region);

		      shadow_region =
			mb_wm_comp_mgr_xrender_client_border_size (c,
								   geom.x, geom.y);

		      XFixesIntersectRegion (wm->xdpy, shadow_region,
					     dc->border_clip, shadow_region );

		      XFixesSetPictureClipRegion (wm->xdpy, priv->root_buffer,
						  0, 0, shadow_region);

		      if (c->is_argb32)
			XRenderComposite (wm->xdpy, PictOpOver,
					  dc->picture, None,
					  priv->root_buffer,
					  win_geom->x, win_geom->y, 0, 0,
					  win_geom->x + geom.x,
					  win_geom->y + geom.y,
					  win_geom->width, win_geom->height);
		      else
			XRenderComposite (wm->xdpy, PictOpOver,
					  dc->picture, priv->trans_picture,
					  priv->root_buffer,
					  win_geom->x, win_geom->y, 0, 0,
					  win_geom->x + geom.x,
					  win_geom->y + geom.y,
					  win_geom->width, win_geom->height);
		    }

		  XFixesDestroyRegion (wm->xdpy, shadow_region);
		}
	      else 		/* GAUSSIAN */
		{
		  MBGeometry * win_geom = & wmc_temp->window->geometry;

		  XFixesSetPictureClipRegion (wm->xdpy, priv->root_buffer,
					      0, 0, dc->border_clip);

		  if (is_translucent)
		    {
		      /* No shadows currently for transparent windows */
		      XRenderComposite (wm->xdpy, PictOpOver,
					dc->picture, priv->trans_picture,
					priv->root_buffer,
					win_geom->x, win_geom->y, 0, 0,
					win_geom->x + geom.x,
					win_geom->y + geom.y,
					win_geom->width, win_geom->height);
		    }
		  else
		    {
		      /* Combine pregenerated shadow tiles */
		      shadow_pic =
			mb_wm_comp_mgr_xrender_shadow_gaussian_make_picture (mgr,
				geom.width + priv->shadow_padding_width,
				geom.height + priv->shadow_padding_height);

		      XRenderComposite (wm->xdpy, PictOpOver,
					priv->black_picture,
					shadow_pic,
					priv->root_buffer,
					win_geom->x, win_geom->y, 0, 0,
					geom.x + priv->shadow_dx,
					geom.y + priv->shadow_dy,
					geom.width +
					priv->shadow_padding_width,
					geom.height +
					priv->shadow_padding_height);

		      XRenderFreePicture (wm->xdpy, shadow_pic);
		    }
		}
	    }
	}
    }

  XFixesSetPictureClipRegion (wm->xdpy, priv->root_buffer, 0, 0, None);

  XRenderComposite (wm->xdpy, PictOpSrc, priv->root_buffer, None,
		    priv->root_picture,
		    0, 0, 0, 0, 0, 0, wm->xdpy_width, wm->xdpy_height);

  if (destroy_region)
    XDamageDestroy (wm->xdpy, region);
}

MBWMCompMgr *
mb_wm_comp_mgr_xrender_new (MBWindowManager *wm)
{
  MBWMObject *mgr;

  mgr = mb_wm_object_new (MB_WM_TYPE_COMP_MGR_DEFAULT, MBWMObjectPropWm, wm, NULL);

  return MB_WM_COMP_MGR (mgr);
}

