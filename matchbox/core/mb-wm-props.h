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

#ifndef _HAVE_MB_WM_PROPS_H
#define _HAVE_MB_WM_PROPS_H

MBWMCookie
mb_wm_property_req (MBWindowManager *wm,
		    Window           win,
		    Atom             property,
		    long             offset,
		    long             length,
		    Bool             delete,
		    Atom             req_type);

Status
mb_wm_property_reply (MBWindowManager  *wm,
		      MBWMCookie        cookie,
		      Atom             *actual_type_return,
		      int              *actual_format_return,
		      unsigned long    *nitems_return,
		      unsigned long    *bytes_after_return,
		      unsigned char   **prop_return,
		      int              *x_error_code);

void*
mb_wm_property_get_reply_and_validate (MBWindowManager  *wm,
				       MBWMCookie        cookie,
				       Atom              expected_type,
				       int               expected_format,
				       int               expected_n_items,
				       int              *n_items_ret,
				       int              *x_error_code);
Bool
mb_wm_property_have_reply (MBWindowManager     *wm,
			   MBWMCookie           cookie);

/* FIXME: mb_wm_xwin_* calls to go else where */

MBWMCookie
mb_wm_xwin_get_attributes (MBWindowManager   *wm,
			   Window             win);

MBWMCookie
mb_wm_xwin_get_geometry (MBWindowManager   *wm,
			 Drawable            d);

MBWMClientWindowAttributes*
mb_wm_xwin_get_attributes_reply (MBWindowManager   *wm,
				 MBWMCookie         cookie,
				 int               *x_error_code);
Status
mb_wm_xwin_get_geometry_reply (MBWindowManager   *wm,
			       XasCookie          cookie,
			       MBGeometry        *geom_return,
			       unsigned int      *border_width_return,
			       unsigned int      *depth_return,
			       int               *x_error_code);

/* Utils */

#define mb_wm_property_cardinal_req(wm, win, prop)                   \
         mb_wm_property_req ((wm),                                   \
			     (win),                                  \
			     (prop),                                 \
			     0,     /* offset */                     \
			     1024L, /* Length, FIXME: Check this */  \
			     False,                                  \
			     XA_CARDINAL);

#define mb_wm_property_atom_req(wm, win, prop)                       \
         mb_wm_property_req ((wm),                                   \
			     (win),                                  \
			     (prop),                                 \
			     0,     /* offset */                     \
			     1024L, /* Length, FIXME: Check this */  \
			     False,                                  \
			     XA_ATOM);

#define mb_wm_property_utf8_req(wm, win, prop)                       \
         mb_wm_property_req ((wm),                                   \
			     (win),                                  \
			     (prop),                                 \
			     0,     /* offset */                     \
			     1024L, /* Length, FIXME: Check this */  \
			     False,                                  \
			     (wm)->atoms[MBWM_ATOM_UTF8_STRING]);

/**
 * Sets the name of a window, the WM_NAME property.
 */
void
mb_wm_rename_window (MBWindowManager *wm,
		     Window win,
		     const char *name);

/**
 * Returns true iff the window is a sub-dialogue.
 * \bug this should be removed now; it's unnecessary
 */
int
mb_window_is_secondary (MBWindowManager *wm, Window w);


#endif
