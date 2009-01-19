/* Asynchronous Xlib hack utiltys lib
 *
 * Copyright (c) 2005 Matthew Allum
 * 
 * Contains portions of code from Metacity and Xlib.
 *
 * Copyright (C) 2002 Havoc Pennington
 * Copyright (C) 1986, 1998  The Open Group
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation.
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of The Open Group shall not be
 * used in advertising or otherwise to promote the sale, use or other dealings
 * in this Software without prior written authorization from The Open Group.
 */

#ifndef _HAVE_XAS_H
#define _HAVE_XAS_H

#include <X11/Xlib.h>
#include <X11/Xutil.h>

typedef struct  XasContext XasContext;
typedef unsigned long      XasCookie;
typedef struct XasWindowAttributes XasWindowAttributes;

/**
 * XWindowAttributes without geom info; must be kept identical to
 * MBWMClientWindowAttributes
 */
struct XasWindowAttributes {
  Visual *visual;         
  Window root;            
  int class;              
  int bit_gravity;        
  int win_gravity;        
  int backing_store;      
  unsigned long backing_planes;
  unsigned long backing_pixel;
  Bool save_under;
  Colormap colormap;
  Bool map_installed;
  int map_state;
  long all_event_masks;
  long your_event_mask;
  long do_not_propagate_mask;
  Bool override_redirect;
};

XasContext*
xas_context_new(Display *xdpy);

void
xas_context_destroy(XasContext *ctx);

XasCookie
xas_get_property(XasContext *ctx,
		 Window      win,
		 Atom        property,
		 long        offset,
		 long        length,
		 Bool        delete,
		 Atom        req_type);

Status
xas_get_property_reply(XasContext          *ctx, 
		       XasCookie            cookie,
		       Atom                *actual_type_return, 
		       int                 *actual_format_return, 
		       unsigned long       *nitems_return, 
		       unsigned long       *bytes_after_return, 
		       unsigned char      **prop_return,
		       int                 *x_error_code);

XasCookie
xas_get_window_attributes(XasContext        *ctx,
			  Window             win);

XasWindowAttributes*
xas_get_window_attributes_reply(XasContext          *ctx, 
				XasCookie            cookie,
				int                 *x_error_code);

XasCookie
xas_get_geometry(XasContext        *ctx,
		 Drawable           d);

Status 
xas_get_geometry_reply (XasContext   *ctx, 
			XasCookie     cookie,  
			int          *x_return, 
			int          *y_return, 
			unsigned int *width_return,
			unsigned int *height_return, 
			unsigned int *border_width_return,
			unsigned int *depth_return,
			int          *x_error_code);
Bool
xas_have_reply(XasContext          *ctx, 
	       XasCookie            cookie);

#endif
