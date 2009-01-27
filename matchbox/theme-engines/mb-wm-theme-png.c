#include <math.h>
#include <png.h>

#include "mb-wm-theme-png.h"
#include "mb-wm-theme-xml.h"

#include "../client-types/mb-wm-client-dialog.h"

#include <X11/Xft/Xft.h>
#include <glib-object.h>

#ifdef HAVE_XEXT
#include <X11/extensions/shape.h>
#endif

static int
mb_wm_theme_png_ximg (MBWMThemePng * theme, const char * img);

static unsigned char*
mb_wm_theme_png_load_file (const char *file, int *width, int *height);

static void
mb_wm_theme_png_paint_decor (MBWMTheme *theme, MBWMDecor *decor);

static void
mb_wm_theme_png_paint_button (MBWMTheme *theme, MBWMDecorButton *button);

static void
mb_wm_theme_png_get_decor_dimensions (MBWMTheme *, MBWindowManagerClient *,
				      int*, int*, int*, int*);

static MBWMDecor *
mb_wm_theme_png_create_decor (MBWMTheme*, MBWindowManagerClient *,
			      MBWMDecorType);

static void
mb_wm_theme_png_resize_decor (MBWMTheme *theme, MBWMDecor *decor);

static void
mb_wm_theme_png_set_left_padding (MBWMTheme *theme, MBWMDecor *decor,
                                  int new_padding);

static void
mb_wm_theme_png_get_button_size (MBWMTheme *, MBWMDecor *,
				 MBWMDecorButtonType, int *, int *);

static void
mb_wm_theme_png_get_button_position (MBWMTheme *, MBWMDecor *,
				     MBWMDecorButtonType,
				     int *, int *);

static void
mb_wm_theme_png_class_init (MBWMObjectClass *klass)
{
  MBWMThemeClass *t_class = MB_WM_THEME_CLASS (klass);

  t_class->paint_decor           = mb_wm_theme_png_paint_decor;
  t_class->paint_button          = mb_wm_theme_png_paint_button;
  t_class->decor_dimensions      = mb_wm_theme_png_get_decor_dimensions;
  t_class->button_size           = mb_wm_theme_png_get_button_size;
  t_class->button_position       = mb_wm_theme_png_get_button_position;
  t_class->create_decor          = mb_wm_theme_png_create_decor;
  t_class->resize_decor          = mb_wm_theme_png_resize_decor;
  t_class->set_left_padding      = mb_wm_theme_png_set_left_padding;

#if MBWM_WANT_DEBUG
  klass->klass_name = "MBWMThemePng";
#endif
}

static void
mb_wm_theme_png_destroy (MBWMObject *obj)
{
  MBWMThemePng * theme = MB_WM_THEME_PNG (obj);
  Display * dpy = MB_WM_THEME (obj)->wm->xdpy;

  XRenderFreePicture (dpy, theme->xpic);
  XFreePixmap (dpy, theme->xdraw);

  if (theme->shape_mask)
    XFreePixmap (dpy, theme->shape_mask);
}

static int
mb_wm_theme_png_init (MBWMObject *obj, va_list vap)
{
  MBWMThemePng     *p_theme = MB_WM_THEME_PNG (obj);
  MBWMTheme        *theme   = MB_WM_THEME (obj);
  MBWMObjectProp    prop;
  char             *img = NULL;
#if USE_PANGO
  Display          *xdpy    = theme->wm->xdpy;
  int               xscreen = theme->wm->xscreen;
#endif

  prop = va_arg(vap, MBWMObjectProp);
  while (prop)
    {
      switch (prop)
	{
	case MBWMObjectPropThemeImg:
	  img = va_arg(vap, char *);
	  break;
	default:
	  MBWMO_PROP_EAT (vap, prop);
	}

      prop = va_arg(vap, MBWMObjectProp);
    }

  if (!img || !mb_wm_theme_png_ximg (p_theme, img))
    return 0;

#if USE_PANGO
  p_theme->context = pango_xft_get_context (xdpy, xscreen);
  p_theme->fontmap = pango_xft_get_font_map (xdpy, xscreen);
#endif

  return 1;
}

int
mb_wm_theme_png_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (MBWMThemePngClass),
	sizeof (MBWMThemePng),
	mb_wm_theme_png_init,
	mb_wm_theme_png_destroy,
	mb_wm_theme_png_class_init
      };

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_THEME, 0);
    }

  return type;
}

/**
 * A decor within an MBWMThemePng.  Returned by mb_wm_decor_get_theme_data().
 */
struct DecorData
{
  Pixmap    xpix;
  Pixmap    shape_mask;
  GC        gc_mask;
  XftDraw  *xftdraw;
  XftColor  clr;
#if USE_PANGO
  PangoFont *font;
#else
  XftFont  *font;
#endif
};

static void
decordata_free (MBWMDecor * decor, void *data)
{
  struct DecorData * dd = data;
  Display * xdpy = decor->parent_client->wmref->xdpy;

  XFreePixmap (xdpy, dd->xpix);

  if (dd->shape_mask)
    XFreePixmap (xdpy, dd->shape_mask);

  if (dd->gc_mask)
    XFreeGC (xdpy, dd->gc_mask);

  XftDrawDestroy (dd->xftdraw);

#if USE_PANGO
  if (dd->font)
    g_object_unref (dd->font);
#else
  if (dd->font)
    XftFontClose (xdpy, dd->font);
#endif

  free (dd);
}

/**
 * A button within an MBWMThemePng.
 */
struct ButtonData
{
  /** Pixmap for the button's inactive state */
  Pixmap    xpix_i;
  XftDraw  *xftdraw_i;
  /** Pixmap for the button's active state */
  Pixmap    xpix_a;
  XftDraw  *xftdraw_a;
};

static void
buttondata_free (MBWMDecorButton * button, void *data)
{
  struct ButtonData * bd = data;
  Display * xdpy = button->decor->parent_client->wmref->xdpy;

  XFreePixmap (xdpy, bd->xpix_i);
  XftDrawDestroy (bd->xftdraw_i);
  XFreePixmap (xdpy, bd->xpix_a);
  XftDrawDestroy (bd->xftdraw_a);

  free (bd);
}

#if !USE_PANGO
static XftFont *
xft_load_font(MBWMDecor * decor, MBWMXmlDecor *d)
{
  char desc[512];
  XftFont *font;
  Display * xdpy = decor->parent_client->wmref->xdpy;
  int       xscreen = decor->parent_client->wmref->xscreen;
  int       font_size = d->font_size ? d->font_size : 18;

  if (d->font_units == MBWMXmlFontUnitsPixels)
    {
      font_size = mb_wm_util_pixels_to_points (decor->parent_client->wmref,
					       font_size);
    }

  snprintf (desc, sizeof (desc), "%s-%i",
	    d->font_family ? d->font_family : "Sans",
	    font_size);

  font = XftFontOpenName (xdpy, xscreen, desc);

  return font;
}
#endif

static void
mb_wm_theme_png_paint_button (MBWMTheme *theme, MBWMDecorButton *button)
{
  MBWMDecor              * decor;
  MBWMThemePng           * p_theme = MB_WM_THEME_PNG (theme);
  MBWindowManagerClient  * client;
  MBWMClientType           c_type;;
  MBWMXmlClient          * c;
  MBWMXmlDecor           * d;
  MBWMXmlButton          * b;

  /*
   * We do not paint inactive buttons, as they get painted with the decor
   */
  decor  = button->decor;
  client = mb_wm_decor_get_parent (decor);
  c_type = MB_WM_CLIENT_CLIENT_TYPE (client);

  if ((c = mb_wm_xml_client_find_by_type (theme->xml_clients, c_type)) &&
      (d = mb_wm_xml_decor_find_by_type (c->decors, decor->type))      &&
      (b = mb_wm_xml_button_find_by_type (d->buttons, button->type)))
    {
      Display           * xdpy    = theme->wm->xdpy;
      int                 xscreen = theme->wm->xscreen;
      struct DecorData  * ddata = mb_wm_decor_get_theme_data (decor);
      struct ButtonData * bdata;

      if (!ddata)
	return;

      bdata = mb_wm_decor_button_get_theme_data (button);

      if (!bdata)
	{
	  int a_x = b->active_x > -1 ? b->active_x : b->x;
	  int a_y = b->active_y > -1 ? b->active_y : b->y;

	  int i_x = b->inactive_x > -1 ? b->inactive_x : b->x;
	  int i_y = b->inactive_y > -1 ? b->inactive_y : b->y;

	  bdata = malloc (sizeof (struct ButtonData));

	  bdata->xpix_a = XCreatePixmap(xdpy, decor->xwin,
				      button->geom.width, button->geom.height,
				      DefaultDepth(xdpy, xscreen));

	  bdata->xftdraw_a = XftDrawCreate (xdpy, bdata->xpix_a,
					    DefaultVisual (xdpy, xscreen),
					    DefaultColormap (xdpy, xscreen));

	  bdata->xpix_i = XCreatePixmap(xdpy, decor->xwin,
				      button->geom.width, button->geom.height,
				      DefaultDepth(xdpy, xscreen));

	  bdata->xftdraw_i = XftDrawCreate (xdpy, bdata->xpix_i,
					    DefaultVisual (xdpy, xscreen),
					    DefaultColormap (xdpy, xscreen));

	  /*
	   * If the background color is set for the parent decor, we do a fill
	   * with the parent color first, then composite the decor image over,
	   * and finally composite the button image. (This way we can paint the
	   * button with a simple PictOpSrc, rather than having to do
	   * composting on each draw).
	   */
	  if (d->clr_bg.set)
	    {
              /* Composite the decor over */
	      XRenderComposite (xdpy, PictOpOver,
				p_theme->xpic,
				None,
				XftDrawPicture (bdata->xftdraw_i),
				b->x, b->y, 0, 0, 0, 0, b->width, b->height);

	      /* Copy inactive button to the active one */
	      XRenderComposite (xdpy, PictOpSrc,
				XftDrawPicture (bdata->xftdraw_i),
				None,
				XftDrawPicture (bdata->xftdraw_a),
				0, 0, 0, 0, 0, 0, b->width, b->height);

	      /* Composite inactive and active image on top */
	      XRenderComposite (xdpy, PictOpOver,
				p_theme->xpic,
				None,
				XftDrawPicture (bdata->xftdraw_i),
				i_x, i_y, 0, 0, 0, 0, b->width, b->height);

	      XRenderComposite (xdpy, PictOpOver,
				p_theme->xpic,
				None,
				XftDrawPicture (bdata->xftdraw_a),
				a_x, a_y, 0, 0, 0, 0, b->width, b->height);
	    }
	  else
	    {
	      XRenderComposite (xdpy, PictOpSrc,
				p_theme->xpic,
				None,
				XftDrawPicture (bdata->xftdraw_i),
				d->x + (d->width - b->width),
                                d->y,
                                0, 0, 0, 0, b->width, b->height);

	      XRenderComposite (xdpy, PictOpSrc,
				XftDrawPicture (bdata->xftdraw_i),
				None,
				XftDrawPicture (bdata->xftdraw_a),
				0, 0, 0, 0, 0, 0, b->width, b->height);

	      XRenderComposite (xdpy, PictOpOver,
				p_theme->xpic,
				None,
				XftDrawPicture (bdata->xftdraw_i),
				i_x, i_y, 0, 0, 0, 0, b->width, b->height);

	      XRenderComposite (xdpy, PictOpOver,
				p_theme->xpic,
				None,
				XftDrawPicture (bdata->xftdraw_a),
				a_x, a_y, 0, 0, 0, 0, b->width, b->height);
	    }

	  mb_wm_decor_button_set_theme_data (button, bdata, buttondata_free);

	}

      /* Here we automagically determine if the button should be left or
       * right aligned in the case that a decor is expanded wider than
       * the template image. If the coordinate comes before the point
       * where decor padding is added, it's left aligned else it's
       * right aligned. If no padding hints were given in the theme.xml,
       * then we assume padding happens in the center.
       * Note: we look at pad_length because pad_offset could be 0
       */

      /* FIXME no we don't.  It was broken; fix it. */

      /* For now we put all buttons top right.  This is the right answer
       * at least for back and close; the others we can deal with ad hoc
       * as they come up.
       */

      button->geom.x = mb_wm_decor_get_pack_end_x (decor);
      button->geom.y = 0;
      button->geom.width = b->width;
      button->geom.height = b->height;

      XWindowAttributes attr;
      XGetWindowAttributes( xdpy, decor->xwin, &attr );
      XRenderPictFormat *format = XRenderFindVisualFormat( xdpy, attr.visual );
      gboolean hasAlpha             = ( format->type == PictTypeDirect && format->direct.alphaMask );

      /* We can't use PictOpOver because the target window
       * doesn't have an alpha channel
       */

      XRenderComposite (xdpy, PictOpSrc,
			button->state == MBWMDecorButtonStatePressed ?
			XftDrawPicture (bdata->xftdraw_a) :
			XftDrawPicture (bdata->xftdraw_i),
			None,
			XftDrawPicture (ddata->xftdraw),
			0, 0, 0, 0, button->geom.x, button->geom.y, b->width, b->height);

      XClearWindow (xdpy, decor->xwin);
    }
}

static void
mb_wm_theme_png_resize_decor (MBWMTheme *theme, MBWMDecor *decor)
{
  /*
   * Clear any data we have stored with the theme; this will force
   * resize on the next paint
   */
  mb_wm_decor_set_theme_data (decor, NULL, NULL);
}

static void
mb_wm_theme_png_set_left_padding (MBWMTheme *theme, MBWMDecor *decor,
                                  int new_padding)
{
    g_debug ("Setting left padding to %d", new_padding);
    left_padding = new_padding;
    mb_wm_theme_png_resize_decor (theme, decor);
}

static void
mb_wm_theme_png_paint_decor (MBWMTheme *theme, MBWMDecor *decor)
{
  MBWMThemePng           * p_theme = MB_WM_THEME_PNG (theme);
  MBWindowManagerClient  * client = decor->parent_client;
  MBWMClientType           c_type = MB_WM_CLIENT_CLIENT_TYPE (client);
  MBWMXmlClient          * c;
  MBWMXmlDecor           * d;
  Display		 * xdpy    = theme->wm->xdpy;
  int			   xscreen = theme->wm->xscreen;
  struct DecorData	 * data = mb_wm_decor_get_theme_data (decor);
  const char		 * title;
  int			   x, y;
  int			   operator = PictOpSrc;
  Bool			   shaped;

  if (!((c = mb_wm_xml_client_find_by_type (theme->xml_clients, c_type)) &&
        (d = mb_wm_xml_decor_find_by_type (c->decors, decor->type))))
    return;

#ifdef HAVE_XEXT
  shaped = theme->shaped && c->shaped && !mb_wm_client_is_argb32 (client);
#endif

  if (data && (mb_wm_decor_get_dirty_state (decor) & MBWMDecorDirtyTitle))
    {
      /*
       * If the decor title is dirty, and we already have the data,
       * free it and recreate (since the old title is already composited
       * in the cached image).
       */
      mb_wm_decor_set_theme_data (decor, NULL, NULL);
      data = NULL;
    }

  if (!data)
    {
      XRenderColor rclr;

      data = mb_wm_util_malloc0 (sizeof (struct DecorData));
      data->xpix = XCreatePixmap(xdpy, decor->xwin,
				 decor->geom.width, decor->geom.height,
				 DefaultDepth(xdpy, xscreen));


#ifdef HAVE_XEXT
      if (shaped)
	{
	  data->shape_mask =
	    XCreatePixmap(xdpy, decor->xwin,
			  decor->geom.width, decor->geom.height, 1);

	  data->gc_mask = XCreateGC (xdpy, data->shape_mask, 0, NULL);
	}
#endif
      data->xftdraw = XftDrawCreate (xdpy, data->xpix,
				     DefaultVisual (xdpy, xscreen),
				     DefaultColormap (xdpy, xscreen));

      /*
       * If the background color is set, we fill the pixmaps with it,
       * and then overlay the the PNG image over (this allows a theme
       * to provide a monochromatic PNG that can be toned, e.g., Sato)
       */
      if (d->clr_bg.set)
	{
	  XRenderColor rclr2;

	  operator = PictOpOver;

	  rclr2.red   = (int)(d->clr_bg.r * (double)0xffff);
	  rclr2.green = (int)(d->clr_bg.g * (double)0xffff);
	  rclr2.blue  = (int)(d->clr_bg.b * (double)0xffff);

	  XRenderFillRectangle (xdpy, PictOpSrc,
				XftDrawPicture (data->xftdraw), &rclr2,
				0, 0,
				decor->geom.width, decor->geom.height);
	}

      rclr.red = 0;
      rclr.green = 0;
      rclr.blue  = 0;
      rclr.alpha = 0xffff;

      if (d->clr_fg.set)
	{
	  rclr.red   = (int)(d->clr_fg.r * (double)0xffff);
	  rclr.green = (int)(d->clr_fg.g * (double)0xffff);
	  rclr.blue  = (int)(d->clr_fg.b * (double)0xffff);
	}

      XftColorAllocValue (xdpy, DefaultVisual (xdpy, xscreen),
			  DefaultColormap (xdpy, xscreen),
			  &rclr, &data->clr);

#if USE_PANGO
      {
	PangoFontDescription * pdesc;
	char desc[512];

	snprintf (desc, sizeof (desc), "%s %i%s",
		  d->font_family ? d->font_family : "Sans",
		  d->font_size ? d->font_size : 18,
		  d->font_units == MBWMXmlFontUnitsPoints ? "" : "px");

	pdesc = pango_font_description_from_string (desc);

	data->font = pango_font_map_load_font (p_theme->fontmap,
					       p_theme->context,
					       pdesc);

	pango_font_description_free (pdesc);
      }
#else
      data->font = xft_load_font (decor, d);
#endif
      XSetWindowBackgroundPixmap(xdpy, decor->xwin, data->xpix);

      mb_wm_decor_set_theme_data (decor, data, decordata_free);
    }

  /*
   * Since we want to support things like rounded corners, but still
   * have the decor resizable, we need to paint it in stages
   *
   * We assume that the decor image is exact in it's major axis,
   * i.e., North and South decors provide image of the exactly correct
   * height, and West and East of width.
   */
  if (decor->type == MBWMDecorTypeNorth ||
      decor->type == MBWMDecorTypeSouth)
    {
      if (decor->geom.width < d->width)
	{
	  /* The decor is smaller than the template, cut bit from the
	   * midle
	   */
	  int width1 = decor->geom.width / 2;
	  int width2 = decor->geom.width - width1;
	  int x2     = d->x + d->width - width2;

      XRenderComposite(xdpy, operator,
			   p_theme->xpic,
			   None,
			   XftDrawPicture (data->xftdraw),
			   d->x, d->y, 0, 0, 0, 0,
			   width1, d->height);

	  XRenderComposite(xdpy, operator,
			   p_theme->xpic,
			   None,
			   XftDrawPicture (data->xftdraw),
			   x2 , d->y, 0, 0,
			   width1, 0,
			   width2, d->height);

#ifdef HAVE_XEXT
	  if (shaped)
	    {
	      XCopyArea (xdpy, p_theme->shape_mask, data->shape_mask,
			 data->gc_mask,
			 d->x, d->y, width1, d->height, 0, 0);
	      XCopyArea (xdpy, p_theme->shape_mask, data->shape_mask,
			 data->gc_mask,
			 x2, d->y, width2, d->height, width1, 0);
	    }
#endif
	}
      else if (decor->geom.width == d->width)
	{
	  /* Exact match */
	  XRenderComposite(xdpy, operator,
			   p_theme->xpic,
			   None,
			   XftDrawPicture (data->xftdraw),
			   d->x, d->y, 0, 0,
			   0, 0, d->width, d->height);

#ifdef HAVE_XEXT
	  if (shaped)
	    {
	      XCopyArea (xdpy, p_theme->shape_mask, data->shape_mask,
			 data->gc_mask,
			 d->x, d->y, d->width, d->height, 0, 0);
	    }
#endif
	}
      else
	{
	  /* The decor is bigger than the template, draw extra bit from
	   * the middle
	   */
	  int pad_offset = d->pad_offset;
	  int pad_length = d->pad_length;
	  int gap_length = decor->geom.width - d->width;

	  if (!pad_length)
	    {
	      pad_length =
		decor->geom.width > 30 ? 10 : decor->geom.width / 4 + 1;
	      pad_offset = (d->width / 2) - (pad_length / 2);
	    }

	  XRenderComposite(xdpy, operator,
			   p_theme->xpic,
			   None,
			   XftDrawPicture (data->xftdraw),
			   d->x, d->y, 0, 0,
			   0, 0,
			   pad_offset, d->height);

	  /* TODO: can we do this as one scaled operation? */
	  for (x = pad_offset; x < pad_offset + gap_length; x += pad_length)
	    XRenderComposite(xdpy, operator,
			     p_theme->xpic,
			     None,
			     XftDrawPicture (data->xftdraw),
			     d->x + pad_offset, d->y, 0, 0,
			     x, 0,
			     pad_length,
			     d->height);

	  XRenderComposite(xdpy, operator,
			   p_theme->xpic,
			   None,
			   XftDrawPicture (data->xftdraw),
			   d->x + pad_offset, d->y, 0, 0,
			   pad_offset + gap_length, 0,
			   d->width - pad_offset, d->height);

#ifdef HAVE_XEXT
	  if (shaped)
	    {
	      XCopyArea (xdpy, p_theme->shape_mask, data->shape_mask,
			 data->gc_mask,
			 d->x, d->y,
			 pad_offset, d->height,
			 0, 0);

	      for (x = pad_offset; x < pad_offset + gap_length; x += pad_length)
		XCopyArea (xdpy, p_theme->shape_mask, data->shape_mask,
			   data->gc_mask,
			   d->x + pad_offset, d->y,
			   d->width - pad_offset, d->height,
			   x, 0);

	      XCopyArea (xdpy, p_theme->shape_mask, data->shape_mask,
			 data->gc_mask,
			 d->x + pad_offset, d->y,
			 d->width - pad_offset, d->height,
			 pad_offset + gap_length, 0);
	    }
#endif
	}
    }
  else
    {
      if (decor->geom.height < d->height)
	{
	  /* The decor is smaller than the template, cut bit from the
	   * midle
	   */
	  int height1 = decor->geom.height / 2;
	  int height2 = decor->geom.height - height1;
	  int y2      = d->y + d->height - height2;

	  XRenderComposite(xdpy, operator,
			   p_theme->xpic,
			   None,
			   XftDrawPicture (data->xftdraw),
			   d->x, d->y, 0, 0,
			   0, 0,
			   d->width, height1);

	  XRenderComposite(xdpy, operator,
			   p_theme->xpic,
			   None,
			   XftDrawPicture (data->xftdraw),
			   d->x , y2, 0, 0,
			   0, height1,
			   d->width, height2);

#ifdef HAVE_XEXT
	  if (shaped)
	    {
	      XCopyArea (xdpy, p_theme->shape_mask, data->shape_mask,
			 data->gc_mask,
			 d->x, d->y, d->width, height1, 0, 0);
	      XCopyArea (xdpy, p_theme->shape_mask, data->shape_mask,
			 data->gc_mask,
			 d->x, y2, d->width, height2, 0, height1);
	    }
#endif
	}
      else if (decor->geom.height == d->height)
	{
	  /* Exact match */
	  XRenderComposite(xdpy, operator,
			   p_theme->xpic,
			   None,
			   XftDrawPicture (data->xftdraw),
			   d->x, d->y, 0, 0,
			   0, 0,
			   d->width, d->height);

#ifdef HAVE_XEXT
	  if (shaped)
	    {
	      XCopyArea (xdpy, p_theme->shape_mask, data->shape_mask,
			 data->gc_mask,
			 d->x, d->y, d->width, d->height, 0, 0);
	    }
#endif
	}
      else
	{
	  /* The decor is bigger than the template, draw extra bit from
	   * the middle
	   */
	  int pad_offset = d->pad_offset;
	  int pad_length = d->pad_length;
	  int gap_length = decor->geom.height - d->height;

	  if (!pad_length)
	    {
	      pad_length =
		decor->geom.height > 30 ? 10 : decor->geom.height / 4 + 1;
	      pad_offset = (d->height / 2) - (pad_length / 2);
	    }

	  XRenderComposite(xdpy, operator,
			   p_theme->xpic,
			   None,
			   XftDrawPicture (data->xftdraw),
			   d->x, d->y, 0, 0, 0, 0,
			   d->width, pad_offset);

	  /* TODO: can we do this as one scaled operation? */
	  for (y = pad_offset; y < pad_offset + gap_length; y += pad_length)
	    XRenderComposite(xdpy, operator,
			     p_theme->xpic,
			     None,
			     XftDrawPicture (data->xftdraw),
			     d->x, d->y + pad_offset, 0, 0, 0, y,
			     d->width,
			     pad_length);

	  XRenderComposite(xdpy, operator,
			   p_theme->xpic,
			   None,
			   XftDrawPicture (data->xftdraw),
			   d->x , d->y + pad_offset, 0, 0,
			   0, pad_offset + gap_length,
			   d->width, d->height - pad_offset);

#ifdef HAVE_XEXT
	  if (shaped)
	    {
	      XCopyArea (xdpy, p_theme->shape_mask, data->shape_mask,
			 data->gc_mask,
			 d->x, d->y,
			 d->width, pad_offset,
			 0, 0);

	      for (y = pad_offset; y < pad_offset + gap_length; y += pad_length)
		XCopyArea (xdpy, p_theme->shape_mask, data->shape_mask,
			   data->gc_mask,
			   d->x, d->y + pad_offset,
			   d->width, pad_length,
			   0, y);

	      XCopyArea (xdpy, p_theme->shape_mask, data->shape_mask,
			 data->gc_mask,
			 d->x, d->y + pad_offset,
			 d->width, d->height - pad_offset,
			 0, pad_offset + gap_length);
	    }
#endif
	}
    }

  if (d->show_title &&
      (title = mb_wm_client_get_name (client)) &&
      data->font)
    {
      XRectangle rec;

      int pack_end_x = mb_wm_decor_get_pack_end_x (decor);
      int west_width = mb_wm_client_frame_west_width (client);
      int y, ascent, descent;
      int len = strlen (title);
      int is_secondary_dialog;
      int centering_padding = 0;

      is_secondary_dialog = mb_window_is_secondary(theme->wm, client->window->xwindow);

#if USE_PANGO
      PangoFontMetrics * mtx;
      PangoGlyphString * glyphs;
      GList            * items, *l;
      PangoRectangle     rect;
      int                xoff = 0;

      mtx = pango_font_get_metrics (data->font, NULL);

      ascent  = PANGO_PIXELS (pango_font_metrics_get_ascent (mtx));
      descent = PANGO_PIXELS (pango_font_metrics_get_descent (mtx));

      pango_font_metrics_unref (mtx);
#else
      ascent  = data->font->ascent;
      descent = data->font->descent;
#endif

      y = (decor->geom.height - (ascent + descent)) / 2 + ascent;

      rec.x = left_padding;
      rec.y = 0;
      rec.width = pack_end_x - 2;
      rec.height = d->height;

      XftDrawSetClipRectangles (data->xftdraw, 0, 0, &rec, 1);

#if USE_PANGO
      glyphs = pango_glyph_string_new ();

      /*
       * Run the pango rendering pipeline on this text and draw with
       * the xft backend (why Pango does not provide a convenience
       * API for something as common as drawing a string escapes me).
       */
      items = pango_itemize (p_theme->context, title, 0, len, NULL, NULL);

      l = items;
      while (l)
	{
	  PangoItem * item = l->data;

	  item->analysis.font = data->font;

	  pango_shape (title, len, &item->analysis, glyphs);

	  if (is_secondary_dialog)
	    {
	      PangoRectangle font_extents;

	      pango_glyph_string_extents (glyphs,
					  data->font,
					  NULL,
				  &font_extents);

	      centering_padding = (rec.width - font_extents.width) / 2;
	    }

	  pango_xft_render (data->xftdraw,
			    &data->clr,
			    data->font,
			    glyphs,
			    xoff + west_width + left_padding,
			    y);

	  /* Advance position */
	  pango_glyph_string_extents (glyphs, data->font, NULL, &rect);
	  xoff += PANGO_PIXELS (rect.width);

	  l = l->next;
	}

      if (glyphs)
	pango_glyph_string_free (glyphs);

      g_list_free (items);
#else

      if (is_secondary_dialog)
	{
	  XGlyphInfo extents;

	  XftTextExtentsUtf8 (xdpy,
			  data->font,
			  title, len,
			  &extents);
	  centering_padding = (rec.width - extents.width) / 2;
	}

      XftDrawStringUtf8(data->xftdraw,
			&data->clr,
			data->font,
			centering_padding?
			west_width + centering_padding:
			west_width + left_padding,
			y,
			title, len);
#endif

      /* Unset the clipping rectangle */
      rec.width = decor->geom.width;
      rec.height = decor->geom.height;

      XftDrawSetClipRectangles (data->xftdraw, 0, 0, &rec, 1);
    }

#ifdef HAVE_XEXT
  if (shaped)
    {
      XShapeCombineMask (xdpy, decor->xwin,
			 ShapeBounding, 0, 0,
			 data->shape_mask, ShapeSet);

      XShapeCombineShape (xdpy,
			  client->xwin_frame,
			  ShapeBounding, decor->geom.x, decor->geom.y,
			  decor->xwin,
			  ShapeBounding, ShapeUnion);
    }
#endif
  XClearWindow (xdpy, decor->xwin);
}

static void
construct_buttons (MBWMThemePng * theme, MBWMDecor * decor, MBWMXmlDecor *d)
{
  MBWindowManagerClient *client = decor->parent_client;
  MBWindowManager       *wm     = client->wmref;
  MBWMDecorButton       *button;

  if (d)
    {
      MBWMList * l = d->buttons;
      while (l)
	{
	  MBWMXmlButton * b = l->data;

	  button = mb_wm_decor_button_stock_new (wm,
						 b->type,
						 b->packing,
						 decor,
						 0);

	  mb_wm_decor_button_show (button);
	  mb_wm_object_unref (MB_WM_OBJECT (button));

	  l = l->next;
	}
    }
}

static MBWMDecor *
mb_wm_theme_png_create_decor (MBWMTheme             *theme,
			      MBWindowManagerClient *client,
			      MBWMDecorType          type)
{
  MBWMClientType   c_type = MB_WM_CLIENT_CLIENT_TYPE (client);
  MBWMDecor       *decor = NULL;
  MBWindowManager *wm = client->wmref;
  MBWMXmlClient   *c;

  if ((c = mb_wm_xml_client_find_by_type (theme->xml_clients, c_type)))
    {
      MBWMXmlDecor *d;

      d = mb_wm_xml_decor_find_by_type (c->decors, type);

      if (d)
	{
	  decor = mb_wm_decor_new (wm, type);
	  decor->absolute_packing = True;
	  mb_wm_decor_attach (decor, client);
	  construct_buttons (MB_WM_THEME_PNG (theme), decor, d);
	}
    }

  return decor;
}

static void
mb_wm_theme_png_get_button_size (MBWMTheme             *theme,
				 MBWMDecor             *decor,
				 MBWMDecorButtonType    type,
				 int                   *width,
				 int                   *height)
{
  MBWindowManagerClient * client = decor->parent_client;
  MBWMClientType  c_type = MB_WM_CLIENT_CLIENT_TYPE (client);
  MBWMXmlClient * c;
  MBWMXmlDecor  * d;

  /* FIXME -- assumes button on the north decor only */
  if ((c = mb_wm_xml_client_find_by_type (theme->xml_clients, c_type)) &&
      (d = mb_wm_xml_decor_find_by_type (c->decors, decor->type)))
    {
      MBWMXmlButton * b = mb_wm_xml_button_find_by_type (d->buttons, type);

      if (b)
	{
	  if (width)
	    *width = b->width;

	  if (height)
	    *height = b->height;

	  return;
	}
    }

  if (width)
    *width = 0;

  if (height)
    *height = 0;
}

static void
mb_wm_theme_png_get_button_position (MBWMTheme             *theme,
				     MBWMDecor             *decor,
				     MBWMDecorButtonType    type,
				     int                   *x,
				     int                   *y)
{
  MBWindowManagerClient * client = decor->parent_client;
  MBWMClientType  c_type = MB_WM_CLIENT_CLIENT_TYPE (client);
  MBWMXmlClient * c;
  MBWMXmlDecor  * d;

  /* FIXME -- assumes button on the north decor only */
  if ((c = mb_wm_xml_client_find_by_type (theme->xml_clients, c_type)) &&
      (d = mb_wm_xml_decor_find_by_type (c->decors, decor->type)))
    {
      MBWMXmlButton * b = mb_wm_xml_button_find_by_type (d->buttons, type);

      if (b)
	{
	  if (x)
	    {
	      int button_x;

	      /* Here we automagically determine if the button should be left or
	       * right aligned in the case that a decor is expanded wider than
	       * the template image. If the coordinate comes before the point
	       * where decor padding is added, it's left aligned else it's
	       * right aligned. If no padding hints were given in the theme.xml,
	       * then we assume padding happens in the center.
	       * Note: we look at pad_length because pad_offset could be 0
	       */
	      button_x = b->x - d->x;
	      if (button_x > (d->pad_length ? d->pad_offset : d->width/2) )
		button_x = decor->geom.width - (d->x + d->width - b->x);

	      *x = button_x;
	    }

	  if (y)
	    *y = b->y - d->y;

	  return;
	}
    }

  if (x)
    *x = 0;

  if (y)
    *y = 0;
}

static void
mb_wm_theme_png_get_decor_dimensions (MBWMTheme             *theme,
				      MBWindowManagerClient *client,
				      int                   *north,
				      int                   *south,
				      int                   *west,
				      int                   *east)
{
  MBWMClientType  c_type = MB_WM_CLIENT_CLIENT_TYPE (client);
  MBWMXmlClient * c;
  MBWMXmlDecor  * d;

  /* FIXME -- assumes button on the north decor only */
  if ((c = mb_wm_xml_client_find_by_type (theme->xml_clients, c_type)))
    {
      if (north)
	{
	  d = mb_wm_xml_decor_find_by_type (c->decors, MBWMDecorTypeNorth);

	  if (d)
	    *north = d->height;
	  else
	    *north = 0;
	}

      if (south)
	{
	  d = mb_wm_xml_decor_find_by_type (c->decors, MBWMDecorTypeSouth);

	  if (d)
	    *south = d->height;
	  else
	    *south = 0;
	}

      if (west)
	{
	  d = mb_wm_xml_decor_find_by_type (c->decors, MBWMDecorTypeWest);

	  if (d)
	    *west = d->width;
	  else
	    *west = 0;
	}

      if (east)
	{
	  d = mb_wm_xml_decor_find_by_type (c->decors, MBWMDecorTypeEast);

	  if (d)
	    *east = d->width;
	  else
	    *east = 0;
	}

      return;
    }

  if (north)
    *north = 0;

  if (south)
    *south = 0;

  if (west)
    *west = 0;

  if (east)
    *east = 0;
}

/*
 * From matchbox-keyboard
 */
static unsigned char*
mb_wm_theme_png_load_file (const char *file,
			   int        *width,
			   int        *height)
{
  FILE *fd;
  unsigned char *data;
  unsigned char header[8];
  int  bit_depth, color_type;

  png_uint_32  png_width, png_height, i, rowbytes;
  png_structp png_ptr;
  png_infop info_ptr;
  png_bytep *row_pointers;

  if ((fd = fopen (file, "rb")) == NULL)
    return NULL;

  fread (header, 1, 8, fd);
  if (!png_check_sig (header, 8))
    {
      fclose(fd);
      return NULL;
    }

  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png_ptr)
    {
      fclose(fd);
      return NULL;
    }

  info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr)
    {
      png_destroy_read_struct (&png_ptr, NULL, NULL);
      fclose(fd);
      return NULL;
    }

  if (setjmp (png_ptr->jmpbuf))
    {
      png_destroy_read_struct( &png_ptr, &info_ptr, NULL);
      fclose(fd);
      return NULL;
    }

  png_init_io (png_ptr, fd);
  png_set_sig_bytes (png_ptr, 8);
  png_read_info (png_ptr, info_ptr);
  png_get_IHDR (png_ptr, info_ptr, &png_width, &png_height, &bit_depth,
		&color_type, NULL, NULL, NULL);

  *width  = (int) png_width;
  *height = (int) png_height;

  if (bit_depth == 16)
    png_set_strip_16(png_ptr);

  if (bit_depth < 8)
    png_set_packing(png_ptr);

  if (color_type == PNG_COLOR_TYPE_GRAY  ||
      color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    png_set_gray_to_rgb(png_ptr);

  /* Add alpha */
  if (color_type == PNG_COLOR_TYPE_GRAY ||
      color_type == PNG_COLOR_TYPE_RGB)
    png_set_add_alpha (png_ptr, 0xff, PNG_FILLER_AFTER);

  if (color_type == PNG_COLOR_TYPE_PALETTE ||
      png_get_valid (png_ptr, info_ptr, PNG_INFO_tRNS))
    png_set_expand (png_ptr);

  png_read_update_info (png_ptr, info_ptr);

  /* allocate space for data and row pointers */
  rowbytes = png_get_rowbytes (png_ptr, info_ptr);
  data = (unsigned char*) malloc ((rowbytes * (*height + 1)));
  row_pointers = (png_bytep *) malloc ((*height) * sizeof (png_bytep));

  if (!data || !row_pointers)
    {
      png_destroy_read_struct (&png_ptr, &info_ptr, NULL);

      if (data)
	free (data);

      if (row_pointers)
	free (row_pointers);

      return NULL;
    }

  for (i = 0;  i < *height; i++)
    row_pointers[i] = data + i * rowbytes;

  png_read_image (png_ptr, row_pointers);
  png_read_end (png_ptr, NULL);

  free (row_pointers);
  png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
  fclose(fd);

  return data;
}

static int
mb_wm_theme_png_ximg (MBWMThemePng * theme, const char * img)
{
  MBWindowManager * wm = MB_WM_THEME (theme)->wm;
  Display * dpy = wm->xdpy;
  int       screen = wm->xscreen;

  XImage * ximg, * shape_img = NULL;
  GC       gc, gcm = 0;
  int x;
  int y;
  int width;
  int height;
  XRenderPictFormat       *ren_fmt;
  XRenderPictureAttributes ren_attr;
  unsigned char * p;
  unsigned char * png_data = mb_wm_theme_png_load_file (img, &width, &height);
  Bool shaped = MB_WM_THEME (theme)->shaped;

  if (!png_data || !width || !height)
    return 0;

  ren_fmt = XRenderFindStandardFormat(dpy, PictStandardARGB32);

  theme->xdraw =
    XCreatePixmap (dpy, RootWindow(dpy,screen), width, height, ren_fmt->depth);

  if (shaped)
    theme->shape_mask =
      XCreatePixmap (dpy, RootWindow(dpy,screen), width, height, 1);

  XSync (dpy, False);

  ren_attr.dither          = True;
  ren_attr.component_alpha = True;
  ren_attr.repeat          = False;

  gc  = XCreateGC (dpy, theme->xdraw, 0, NULL);

  if (shaped)
    gcm = XCreateGC (dpy, theme->shape_mask, 0, NULL);

  ximg = XCreateImage (dpy, DefaultVisual (dpy, screen),
		       ren_fmt->depth, ZPixmap,
		       0, NULL, width, height, 32, 0);

  ximg->data = malloc (ximg->bytes_per_line * ximg->height);

  if (shaped)
    {
      shape_img = XCreateImage (dpy, DefaultVisual (dpy, screen),
				1, ZPixmap,
				0, NULL, width, height, 8, 0);

      shape_img->data = malloc (shape_img->bytes_per_line * shape_img->height);
    }

  p = png_data;

  for (y = 0; y < height; y++)
    for (x = 0; x < width; x++)
      {
	unsigned char a, r, g, b;
	/* This is probably the ARM */
	r = *p++; g = *p++; b = *p++; a = *p++;
	r = (r * (a + 1)) / 256;
	g = (g * (a + 1)) / 256;
	b = (b * (a + 1)) / 256;

	XPutPixel (ximg, x, y, (a << 24) | (r << 16) | (g << 8) | b);

	if (shaped)
	  {
	    XPutPixel (shape_img, x, y, a ? 1 : 0);
	  }
      }

  XPutImage (dpy, theme->xdraw, gc, ximg, 0, 0, 0, 0, width, height);

  if (shaped)
    XPutImage (dpy, theme->shape_mask, gcm, shape_img,
	       0, 0, 0, 0, width, height);

  theme->xpic = XRenderCreatePicture (dpy, theme->xdraw, ren_fmt,
				      CPRepeat|CPDither|CPComponentAlpha,
				      &ren_attr);

  free (ximg->data);
  ximg->data = NULL;
  XDestroyImage (ximg);
  XFreeGC (dpy, gc);

  if (shaped)
    {
      free (shape_img->data);
      shape_img->data = NULL;
      XDestroyImage (shape_img);
      XFreeGC (dpy, gcm);
    }

  free (png_data);

  return 1;
}
