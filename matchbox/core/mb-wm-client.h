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

#ifndef _HAVE_MB_CLIENT_H
#define _HAVE_MB_CLIENT_H

#include <matchbox/mb-wm-config.h>

#define MB_WM_CLIENT(c) ((MBWindowManagerClient*)(c))
#define MB_WM_CLIENT_CLASS(c) ((MBWindowManagerClientClass*)(c))
#define MB_WM_TYPE_CLIENT (mb_wm_client_class_type ())
#define MB_WM_CLIENT_XWIN(w) (w)->window->xwindow
#define MB_WM_CLIENT_CLIENT_TYPE(c) \
    (MB_WM_CLIENT_CLASS(MB_WM_OBJECT_GET_CLASS(c))->client_type)

typedef void (*MBWindowManagerClientInitMethod) (MBWindowManagerClient *client);

/**
 * Clients hint to what stacking layer they exist in. By default all
 * transients to that client will also be stacked there.
 */
typedef enum MBWMStackLayerType
{
  MBWMStackLayerUnknown     = 0, /* Transients */
  MBWMStackLayerBottom       , 	 /* Desktop window */
  MBWMStackLayerBottomMid    ,	 /* Panels */
  MBWMStackLayerMid          ,	 /* Apps */
  MBWMStackLayerTopMid       ,	 /* Trans for root dialogs */
  MBWMStackLayerTop          ,	 /* Something else ? */

  /* layers used for device-specific windows (e.g. touchscreen lock) */
  MBWMStackLayerHildon1      ,
  MBWMStackLayerHildon2      ,
  MBWMStackLayerHildon3      ,
  MBWMStackLayerHildon4      ,
  MBWMStackLayerHildon5      ,
  MBWMStackLayerHildon6      ,
  MBWMStackLayerHildon7      ,
  MBWMStackLayerHildon8      ,
  MBWMStackLayerHildon9      ,
  MBWMStackLayerHildon10     ,
  N_MBWMStackLayerTypes
}
MBWMStackLayerType;

/* Clients can also hint to as how they would like to be managed by the
 * layout manager.
 */
typedef enum MBWMClientLayoutHints
  {
    LayoutPrefReserveEdgeNorth = (1<< 0), /* panels */
    LayoutPrefReserveEdgeSouth = (1<< 1),
    LayoutPrefReserveEdgeEast  = (1<< 2),
    LayoutPrefReserveEdgeWest  = (1<< 3),
    LayoutPrefReserveNorth     = (1<< 4), /* Input wins */
    LayoutPrefReserveSouth     = (1<< 5),
    LayoutPrefReserveEast      = (1<< 6),
    LayoutPrefReserveWest      = (1<< 7),
    LayoutPrefGrowToFreeSpace  = (1<< 8), /* Free space left by above   */
    LayoutPrefFullscreen       = (1<< 9), /* Fullscreen and desktop wins */
    LayoutPrefPositionFree     = (1<<10), /* Dialog */
    LayoutPrefVisible          = (1<<11), /* Flag is toggled by stacking */
    LayoutPrefFixedX           = (1<<12), /* X and width are fixed*/
    LayoutPrefFixedY           = (1<<13),
    LayoutPrefOverlaps         = (1<<14), /* stacked over other windows */
    LayoutPrefMovable          = (1<<15),
    LayoutPrefResizable        = (1<<16),
  }
MBWMClientLayoutHints;

typedef enum MBWMClientReqGeomType
  {
    MBWMClientReqGeomDontCommit         = (1 << 1),
    MBWMClientReqGeomIsViaConfigureReq  = (1 << 2),
    MBWMClientReqGeomIsViaUserAction    = (1 << 3),
    MBWMClientReqGeomIsViaLayoutManager = (1 << 4),
    MBWMClientReqGeomForced             = (1 << 5)
  }
MBWMClientReqGeomType;

/* Methods */

typedef  void (*MBWMClientNewMethod) (MBWindowManager       *wm,
				      MBWMClientWindow      *win);

typedef  void (*MBWMClientInitMethod) (MBWindowManager       *wm,
				       MBWindowManagerClient *client,
				       MBWMClientWindow      *win);

typedef  void (*MBWMClientRealizeMethod) (MBWindowManagerClient *client);

typedef  void (*MBWMClientDestroyMethod) (MBWindowManagerClient *client);

typedef  Bool (*MBWMClientGeometryMethod) (MBWindowManagerClient *client,
					   MBGeometry            *new_geometry,
					   MBWMClientReqGeomType  flags);

typedef  void (*MBWMClientStackMethod) (MBWindowManagerClient *client,
					int                    flags);

typedef  void (*MBWMClientShowMethod) (MBWindowManagerClient *client);

typedef  void (*MBWMClientHideMethod) (MBWindowManagerClient *client);

typedef  void (*MBWMClientSyncMethod) (MBWindowManagerClient *client);

typedef  Bool (*MBWMClientFocusMethod)(MBWindowManagerClient *client);

typedef  void (*MBWMClientThemeChangeMethod) (MBWindowManagerClient *client);

typedef  void (*MBWMClientDetransitise) (MBWindowManagerClient *client);

typedef  MBWMStackLayerType (*MBWMClientStackingLayer)(MBWindowManagerClient*);

/**
 * Class of MBWindowManagerClient.
 */
struct MBWindowManagerClientClass
{
  MBWMObjectClass              parent;

  MBWMClientType               client_type;

  MBWMClientRealizeMethod      realize;	 /* create dpy resources / reparent */
  MBWMClientGeometryMethod     geometry; /* requests a gemetry change */
  MBWMClientStackMethod        stack;    /* positions win in stack */
  MBWMClientShowMethod         show;
  MBWMClientHideMethod         hide;
  MBWMClientSyncMethod         sync;     /* sync internal changes to display */
  MBWMClientFocusMethod        focus;
  MBWMClientThemeChangeMethod  theme_change;
  MBWMClientDetransitise       detransitise;
  MBWMClientStackingLayer      stacking_layer;
};

/**
 * One X client, a window of one of the standard MBWMClientType types;
 * in MBWindowManager; contains MBWMClientWindow.
 */
struct MBWindowManagerClient
{
  MBWMObject                   parent;
  /* ### public ### */

  MBWindowManager             *wmref;
  char                        *name;
  MBWMClientWindow            *window;
  Window                       xwin_frame;
  Window                       xwin_modal_blocker;
  MBWMStackLayerType           stacking_layer;
  unsigned long                stacking_hints;

  MBWMClientLayoutHints        layout_hints;

  MBWindowManagerClient       *stacked_above, *stacked_below;

  MBGeometry frame_geometry;  /* FIXME: in ->priv ? */
  /**
   * List of MBWMDecor objects.
   * \bug Why is this not an array?  When do we ever not have four?
   */
  MBWMList                    *decor;
  /**
   * List of MBWindowManagerClient objects which are transient to this
   * object
   */
  MBWMList                    *transients;
  MBWindowManagerClient       *transient_for;

  int                          skip_maps;
  int                          skip_unmaps;

  /* ### Private ### */

  MBWindowManagerClientPriv   *priv;
  unsigned long                sig_prop_change_id;
  unsigned long                ping_cb_id;
  unsigned long                sig_theme_change_id;
  int                          ping_timeout;

  Bool                         is_argb32;

  int                          desktop;

#if ENABLE_COMPOSITE
  MBWMCompMgrClient           *cm_client;
#endif
};

#define mb_wm_client_frame_west_width(c) \
         ((c)->window->geometry.x - (c)->frame_geometry.x)
#define mb_wm_client_frame_east_width(c) \
         (((c)->frame_geometry.x + (c)->frame_geometry.width) \
          - ((c)->window->geometry.x + (c)->window->geometry.width))
#define mb_wm_client_frame_east_x(c) \
          ((c)->window->geometry.x + (c)->window->geometry.width)
#define mb_wm_client_frame_north_height(c) \
         ((c)->window->geometry.y - (c)->frame_geometry.y)
#define mb_wm_client_frame_south_y(c) \
         ((c)->window->geometry.y + (c)->window->geometry.height)
#define mb_wm_client_frame_south_height(c) \
         ( ((c)->frame_geometry.y + (c)->frame_geometry.height) \
          - ((c)->window->geometry.y + (c)->window->geometry.height) )

int
mb_wm_client_class_type ();

MBWMClientWindow*
mb_wm_client_window_new (MBWindowManager *wm, Window window);

MBWindowManagerClient*
mb_wm_client_new (MBWindowManager *wm, MBWMClientWindow *win);

void
mb_wm_client_realize (MBWindowManagerClient *client);

void
mb_wm_client_stack (MBWindowManagerClient *client,
		    int                    flags);
void
mb_wm_client_show (MBWindowManagerClient *client);

void
mb_wm_client_hide (MBWindowManagerClient *client);

Bool
mb_wm_client_focus (MBWindowManagerClient *client);

Bool
mb_wm_client_want_focus (MBWindowManagerClient *client);

void
mb_wm_client_display_sync (MBWindowManagerClient *client);


Bool
mb_wm_client_is_realized (MBWindowManagerClient *client);

Bool
mb_wm_client_request_geometry (MBWindowManagerClient *client,
                               MBGeometry            *new_geometry,
                               MBWMClientReqGeomType  flags);

Bool
mb_wm_client_needs_geometry_sync (MBWindowManagerClient *client);

Bool
mb_wm_client_needs_visibility_sync (MBWindowManagerClient *client);

Bool
mb_wm_client_needs_fullscreen_sync (MBWindowManagerClient *client);

Bool
mb_wm_client_needs_decor_sync (MBWindowManagerClient *client);

Bool
mb_wm_client_needs_configure_request_ack (MBWindowManagerClient *client);

void
mb_wm_client_configure_request_ack_queue (MBWindowManagerClient *client);

Bool
mb_wm_client_needs_sync (MBWindowManagerClient *client);

Bool
mb_wm_client_is_mapped (MBWindowManagerClient *client);

Bool
mb_wm_client_is_map_confirmed (MBWindowManagerClient *client);

Bool
mb_wm_client_is_unmap_confirmed (MBWindowManagerClient *client);

void
mb_wm_client_set_map_confirmed (MBWindowManagerClient *client,
		Bool confirmed);

void
mb_wm_client_set_unmap_confirmed (MBWindowManagerClient *client,
		Bool confirmed);

void
mb_wm_client_get_coverage (MBWindowManagerClient *client,
                           MBGeometry            *coverage);

static MBWMClientLayoutHints
mb_wm_client_get_layout_hints (MBWindowManagerClient *client);

void
mb_wm_client_set_layout_hints (MBWindowManagerClient *client,
                               MBWMClientLayoutHints  hints);

void
mb_wm_client_set_layout_hint (MBWindowManagerClient *client,
			      MBWMClientLayoutHints  hint,
                              Bool                   state);

void
mb_wm_client_stacking_mark_dirty (MBWindowManagerClient *client);

void
mb_wm_client_fullscreen_mark_dirty (MBWindowManagerClient *client);

void
mb_wm_client_geometry_mark_dirty (MBWindowManagerClient *client);

void
mb_wm_client_visibility_mark_dirty (MBWindowManagerClient *client);

void
mb_wm_client_decor_mark_dirty (MBWindowManagerClient *client);

void
mb_wm_client_add_transient (MBWindowManagerClient *client,
			    MBWindowManagerClient *transient);

void
mb_wm_client_remove_transient (MBWindowManagerClient *client,
			       MBWindowManagerClient *transient);

void
mb_wm_client_remove_all_transients (MBWindowManagerClient *client);

MBWindowManagerClient*
mb_wm_client_get_next_focused_client (MBWindowManagerClient *client);

MBWindowManagerClient*
mb_wm_client_get_next_focused_app (MBWindowManagerClient *client);

MBWindowManagerClient*
mb_wm_client_get_last_focused_transient (MBWindowManagerClient *client);

MBWMList*
mb_wm_client_get_transients (MBWindowManagerClient *client);

static MBWindowManagerClient*
mb_wm_client_get_transient_for (MBWindowManagerClient *client);

gboolean
mb_wm_client_is_system_modal (MBWindowManagerClient *client);

const char*
mb_wm_client_get_name (MBWindowManagerClient *client);

void
mb_wm_client_deliver_delete (MBWindowManagerClient *client);

gboolean
mb_wm_client_deliver_message (MBWindowManagerClient   *client,
			      Atom          delivery_atom,
			      unsigned long data0,
			      unsigned long data1,
			      unsigned long data2,
			      unsigned long data3,
			      unsigned long data4);

void
mb_wm_client_deliver_wm_protocol (MBWindowManagerClient *client,
				  Atom protocol);

void
mb_wm_client_shutdown (MBWindowManagerClient *client);

void
mb_wm_client_set_state (MBWindowManagerClient *client,
			MBWMAtom state,
			MBWMClientWindowStateChange state_op);

Bool
mb_wm_client_ping_in_progress (MBWindowManagerClient * client);

void
mb_wm_client_ping_stop (MBWindowManagerClient *client);

void
mb_wm_client_theme_change (MBWindowManagerClient *client);

void
mb_wm_client_detransitise (MBWindowManagerClient *client);

Bool
mb_wm_client_is_iconizing (MBWindowManagerClient *client);

void
mb_wm_client_reset_iconizing (MBWindowManagerClient *client);

void
mb_wm_client_iconize (MBWindowManagerClient *client);

int
mb_wm_client_title_height (MBWindowManagerClient *client);

Bool
mb_wm_client_is_modal (MBWindowManagerClient *client);

Bool
mb_wm_client_owns_xwindow (MBWindowManagerClient *client, Window xwin);

MBWMStackLayerType
mb_wm_client_get_stacking_layer (MBWindowManagerClient *client);

void
mb_wm_client_ping_start (MBWindowManagerClient *client);

Bool
mb_wm_client_is_argb32 (MBWindowManagerClient *client);

void
mb_wm_client_set_desktop (MBWindowManagerClient * client, int desktop);

int
mb_wm_client_get_desktop (MBWindowManagerClient * client);

void
mb_wm_client_desktop_change (MBWindowManagerClient * client, int desktop);

Bool
mb_wm_client_is_hiding_from_desktop (MBWindowManagerClient * client);

void
mb_wm_client_reset_hiding_from_desktop (MBWindowManagerClient * client);

Bool
mb_wm_client_is_visible (MBWindowManagerClient * client);

Bool
mb_wm_client_covers_screen (MBWindowManagerClient * client);

Bool
mb_wm_client_wants_portrait (MBWindowManagerClient * client);

static inline MBWMClientLayoutHints
mb_wm_client_get_layout_hints (MBWindowManagerClient *client)
{
  return (client->window->ewmh_state & MBWMClientWindowEWMHStateFullscreen)
    ? LayoutPrefFullscreen | (client->layout_hints & LayoutPrefVisible)
    : client->layout_hints;
}

static inline MBWindowManagerClient*
mb_wm_client_get_transient_for (MBWindowManagerClient *client)
{
  return client->transient_for;
}

#endif
