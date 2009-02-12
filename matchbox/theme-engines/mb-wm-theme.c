/*
 *  Matchbox Window Manager II - A lightweight window manager not for the
 *                               desktop.
 *
 *  Authored By Tomas Frydrych <tf@o-hand.com>
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

#include "mb-wm-theme.h"
#include "mb-wm-theme-xml.h"
#include "mb-wm-theme-png.h"

#include "../client-types/mb-wm-client-dialog.h"

#include <sys/stat.h>
#include <expat.h>
#include <X11/Xft/Xft.h>
#include <glib-object.h>

#define SIMPLE_FRAME_TITLEBAR_HEIGHT 40
#define SIMPLE_FRAME_EDGE_SIZE 0

/* FIXME! Global variable named like this? What about putting it to struct? */
unsigned int left_padding = 0;

MBWMThemeCustomClientTypeFunc  custom_client_type_func      = NULL;
void                          *custom_client_type_func_data = NULL;

MBWMThemeCustomButtonTypeFunc  custom_button_type_func      = NULL;
void                          *custom_button_type_func_data = NULL;

MBWMThemeCustomThemeTypeFunc   custom_theme_type_func      = NULL;
void                          *custom_theme_type_func_data = NULL;

MBWMThemeCustomThemeAllocFunc  custom_theme_alloc_func      = NULL;

static void
xml_element_start_cb (void *data, const char *tag, const char **expat_attr);

static void
xml_element_end_cb (void *data, const char *tag);

static void
xml_stack_free (MBWMList *stack);

static void
mb_wm_theme_simple_paint_decor (MBWMTheme *theme, MBWMDecor *decor);
static void
mb_wm_theme_simple_paint_button (MBWMTheme *theme, MBWMDecorButton *button);
static void
mb_wm_theme_simple_get_decor_dimensions (MBWMTheme *, MBWindowManagerClient *,
					 int *, int *, int *, int *);
static void
mb_wm_theme_simple_get_button_size (MBWMTheme *, MBWMDecor *,
				    MBWMDecorButtonType, int *, int *);
static void
mb_wm_theme_simple_get_button_position (MBWMTheme *, MBWMDecor *,
					MBWMDecorButtonType, int*, int*);
static MBWMDecor *
mb_wm_theme_simple_create_decor (MBWMTheme *, MBWindowManagerClient *,
				 MBWMDecorType);

static void
mb_wm_theme_class_init (MBWMObjectClass *klass)
{
  MBWMThemeClass *t_class = MB_WM_THEME_CLASS (klass);

  t_class->paint_decor      = mb_wm_theme_simple_paint_decor;
  t_class->paint_button     = mb_wm_theme_simple_paint_button;
  t_class->decor_dimensions = mb_wm_theme_simple_get_decor_dimensions;
  t_class->button_size      = mb_wm_theme_simple_get_button_size;
  t_class->button_position  = mb_wm_theme_simple_get_button_position;
  t_class->create_decor     = mb_wm_theme_simple_create_decor;

#if MBWM_WANT_DEBUG
  klass->klass_name = "MBWMTheme";
#endif
}

static void
mb_wm_theme_destroy (MBWMObject *obj)
{
  MBWMTheme *theme = MB_WM_THEME (obj);

  if (theme->path)
    free (theme->path);

  if (theme->image_filename)
    free (theme->image_filename);

  MBWMList *l = theme->xml_clients;

  while (l)
    {
      MBWMXmlClient * c = l->data;
      MBWMList * n = l->next;
      mb_wm_xml_client_free (c);
      free (l);

      l = n;
    }
}

static int
mb_wm_theme_init (MBWMObject *obj, va_list vap)
{
  MBWMTheme        *theme = MB_WM_THEME (obj);
  MBWindowManager  *wm = NULL;
  MBWMObjectProp    prop;
  MBWMList         *xml_clients = NULL;
  char             *path = NULL;
  char             *image_filename = NULL;
  MBWMColor        *clr_lowlight = NULL;
  MBWMColor        *clr_shadow = NULL;

  prop = va_arg(vap, MBWMObjectProp);
  while (prop)
    {
      switch (prop)
	{
	case MBWMObjectPropWm:
	  wm = va_arg(vap, MBWindowManager *);
	  break;
	case MBWMObjectPropThemePath:
	  path = va_arg(vap, char *);
	  break;
	case MBWMObjectPropThemeImg:
	  image_filename = va_arg(vap, char *);
          break;
	case MBWMObjectPropThemeXmlClients:
	  xml_clients = va_arg(vap, MBWMList *);
	  break;
	case MBWMObjectPropThemeColorLowlight:
	  clr_lowlight = va_arg(vap, MBWMColor *);
	  break;
	case MBWMObjectPropThemeColorShadow:
	  clr_shadow = va_arg(vap, MBWMColor *);
	  break;
	case MBWMObjectPropThemeShadowType:
	  theme->shadow_type = va_arg(vap, int);
	  break;
	case MBWMObjectPropThemeCompositing:
	  theme->compositing = va_arg(vap, int);
	  break;
	case MBWMObjectPropThemeShaped:
	  theme->shaped = va_arg(vap, int);
	  break;

	default:
	  MBWMO_PROP_EAT (vap, prop);
	}

      prop = va_arg(vap, MBWMObjectProp);
    }

  theme->wm = wm;
  theme->xml_clients = xml_clients;

  if (path)
    theme->path = strdup (path);

  if (image_filename)
    theme->image_filename = strdup(image_filename);
  else
    theme->image_filename = 0;

  if (clr_shadow && clr_shadow->set)
    {
      theme->color_shadow.r = clr_shadow->r;
      theme->color_shadow.g = clr_shadow->g;
      theme->color_shadow.b = clr_shadow->b;
      theme->color_shadow.a = clr_shadow->a;
    }
  else
    {
      theme->color_shadow.r = 0.0;
      theme->color_shadow.g = 0.0;
      theme->color_shadow.b = 0.0;
      theme->color_shadow.a = 0.95;
    }

  if (clr_lowlight && clr_lowlight->set)
    {
      theme->color_lowlight.r = clr_lowlight->r;
      theme->color_lowlight.g = clr_lowlight->g;
      theme->color_lowlight.b = clr_lowlight->b;
      theme->color_lowlight.a = clr_lowlight->a;
    }
  else
    {
      theme->color_lowlight.r = 0.0;
      theme->color_lowlight.g = 0.0;
      theme->color_lowlight.b = 0.0;
      theme->color_lowlight.a = 0.55;
    }

  return 1;
}

int
mb_wm_theme_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (MBWMThemeClass),
	sizeof (MBWMTheme),
	mb_wm_theme_init,
	mb_wm_theme_destroy,
	mb_wm_theme_class_init
      };

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_OBJECT, 0);
    }

  return type;
}

Bool
mb_wm_theme_is_button_press_activated (MBWMTheme              *theme,
				       MBWMDecor              *decor,
				       MBWMDecorButtonType    type)
{
  MBWindowManagerClient * client;
  MBWMXmlClient         * c;
  MBWMXmlDecor          * d;
  MBWMXmlButton         * b;
  MBWMClientType          c_type;

  if (!theme || !theme->xml_clients || !decor || !decor->parent_client)
    return False;

  client = decor->parent_client;
  c_type = MB_WM_CLIENT_CLIENT_TYPE (client);

  if ((c = mb_wm_xml_client_find_by_type (theme->xml_clients, c_type)) &&
      (d = mb_wm_xml_decor_find_by_type (c->decors, decor->type)) &&
      (b = mb_wm_xml_button_find_by_type (d->buttons, type)))
    {
      return b->press_activated;
    }

  return False;
}

void
mb_wm_theme_get_button_size (MBWMTheme             *theme,
			     MBWMDecor             *decor,
			     MBWMDecorButtonType    type,
			     int                   *width,
			     int                   *height)
{
  MBWMThemeClass *klass;

  MBWM_ASSERT (decor && decor->parent_client);

  if (!theme || !decor || !decor->parent_client)
    return;

  klass = MB_WM_THEME_CLASS(MB_WM_OBJECT_GET_CLASS (theme));

  if (klass->button_size)
    klass->button_size (theme, decor, type, width, height);
}

/*
 * If the parent decor uses absolute postioning, the returned values
 * are absolute. If the decor does packing, these values are added to
 * calculated button position.
 */
void
mb_wm_theme_get_button_position (MBWMTheme             *theme,
				 MBWMDecor             *decor,
				 MBWMDecorButtonType    type,
				 int                   *x,
				 int                   *y)
{
  MBWMThemeClass *klass;

  MBWM_ASSERT (decor && decor->parent_client);

  if (!theme || !decor || !decor->parent_client)
    return;

  klass = MB_WM_THEME_CLASS(MB_WM_OBJECT_GET_CLASS (theme));

  if (klass->button_position)
    klass->button_position (theme, decor, type, x, y);
  else
    {
      if (x)
	*x = 2;

      if (y)
	*y = 2;
    }
}

void
mb_wm_theme_get_decor_dimensions (MBWMTheme             *theme,
				  MBWindowManagerClient *client,
				  int                   *north,
				  int                   *south,
				  int                   *west,
				  int                   *east)
{
  MBWMThemeClass *klass;

  MBWM_ASSERT (client);

  if (!theme || !client)
    return;

  klass = MB_WM_THEME_CLASS(MB_WM_OBJECT_GET_CLASS (theme));

  if (klass->decor_dimensions)
    klass->decor_dimensions (theme, client, north, south, west, east);
}

void
mb_wm_theme_paint_decor (MBWMTheme *theme, MBWMDecor *decor)
{
  MBWMThemeClass *klass;

  if (!theme)
    return;

  klass = MB_WM_THEME_CLASS(MB_WM_OBJECT_GET_CLASS (theme));

  if (klass->paint_decor)
    klass->paint_decor (theme, decor);
}

void
mb_wm_theme_paint_button (MBWMTheme *theme, MBWMDecorButton *button)
{
  MBWMThemeClass *klass;

  if (!theme)
    return;

  klass = MB_WM_THEME_CLASS(MB_WM_OBJECT_GET_CLASS (theme));

  if (klass->paint_button)
    klass->paint_button (theme, button);
}

Bool
mb_wm_theme_supports (MBWMTheme *theme, MBWMThemeCaps capability)
{
  if (!theme)
    return False;

  return ((capability & theme->caps) != False);
}

typedef enum
{
  XML_CTX_UNKNOWN = 0,
  XML_CTX_THEME,
  XML_CTX_CLIENT,
  XML_CTX_DECOR,
  XML_CTX_BUTTON,
  XML_CTX_IMG,
} XmlCtx;

/**
 * During XML parsing, stores the backtrace of which XML tags we're inside.
 */
struct stack_data
{
  XmlCtx  ctx;
  void   *data;
};

/**
 * The unprocessed result of parsing a theme XML file.
 */
struct expat_data
{
  XML_Parser   par;
  int          theme_type;
  int          version;
  MBWMList     *xml_clients;
  char         *img;
  MBWMList     *stack;
  MBWMColor     color_lowlight;
  MBWMColor     color_shadow;
  MBWMCompMgrShadowType shadow_type;
  Bool          compositing;
  Bool          shaped;
};

/**
 * Loads a theme into memory from disk, and returns it.
 */
MBWMTheme *
mb_wm_theme_new (MBWindowManager * wm, const char * theme_path)
{
  MBWMTheme     *theme = NULL;
  int            theme_type = 0;
  char          *path = NULL;
  char           buf[256];
  XML_Parser     par = NULL;
  FILE          *file = NULL;
  MBWMList      *xml_clients = NULL;
  char          *img = NULL;
  MBWMColor      clr_lowlight;
  MBWMColor      clr_shadow;
  MBWMCompMgrShadowType shadow_type = 0;
  Bool           compositing = False;
  Bool           shaped = False;
  struct stat    st;

  /*
   * If no theme specified, we try to load the default one, if that fails,
   * we automatically fallback on the built-in defaults.
   */
  if (!theme_path)
    theme_path = "default";

  /* Attempt to parse the xml theme, if any, retrieving the theme type
   *
   * NB: We cannot do this in the _init function, since we need to know the
   *     type *before* we can create the underlying object on which the
   *     init method operates.
   */

  if (*theme_path == '/')
    {
      if (!stat (theme_path, &st))
	path = (char *) theme_path;
    }
  else
    {
      const char  *home = getenv("HOME");
      int          size;

      if (home)
	{
	  const char  *fmt = "%s/.themes/%s/matchbox2/theme.xml";

	  size = strlen (theme_path) + strlen (fmt) + strlen (home);
	  path = alloca (size);
	  snprintf (path, size, fmt, home, theme_path);

	  if (stat (path, &st))
	    path = NULL;
	}

      if (!path)
	{
	  const char * fmt = "%s/themes/%s/matchbox2/theme.xml";

	  size = strlen (theme_path) + strlen (fmt) + strlen (DATADIR);
	  path = alloca (size);
	  snprintf (path, size, fmt, DATADIR, theme_path);

	  if (stat (path, &st))
	    path = NULL;
	}

      /* FIXME: temporary fallback for the time before theme package update
       */
      if (!path)
	{
	  const char * fmt = "%s/themes/%s/matchbox/theme.xml";

	  size = strlen (theme_path) + strlen (fmt) + strlen (DATADIR);
	  path = alloca (size);
	  snprintf (path, size, fmt, DATADIR, theme_path);

	  if (stat (path, &st))
	    path = NULL;
	}
    }

  if (path)
    {
      struct expat_data  udata;

      if (!(file = fopen (path, "r")) ||
	  !(par = XML_ParserCreate(NULL)))
	{
	  goto default_theme;
	}

      memset (&udata, 0, sizeof (struct expat_data));
      udata.compositing = True;
      udata.par         = par;

      XML_SetElementHandler (par,
			     xml_element_start_cb,
			     xml_element_end_cb);

      XML_SetUserData(par, (void *)&udata);

      while (fgets (buf, sizeof (buf), file) &&
	     XML_Parse(par, buf, strlen(buf), 0));

      XML_Parse(par, NULL, 0, 1);

      if (udata.version == 2)
	{
	  theme_type  = udata.theme_type;
	  xml_clients = udata.xml_clients;

	  if (udata.img)
	    {
	      if (*udata.img == '/')
		img = udata.img;
	      else
		{
		  int len = strlen (path) + strlen (udata.img);
		  char * s;
		  char * p = malloc (len + 1);
		  strncpy (p, path, len);

		  s = strrchr (p, '/');

		  if (s)
		    {
		      *(s+1) = 0;
		      strcat (p, udata.img);
		    }
		  else
		    {
		      strncpy (p, udata.img, len);
		    }

		  img = p;
		  free (udata.img);
		}
	    }
	}

      clr_lowlight.r   = udata.color_lowlight.r;
      clr_lowlight.g   = udata.color_lowlight.g;
      clr_lowlight.b   = udata.color_lowlight.b;
      clr_lowlight.a   = udata.color_lowlight.a;
      clr_lowlight.set = udata.color_lowlight.set;

      clr_shadow.r   = udata.color_shadow.r;
      clr_shadow.g   = udata.color_shadow.g;
      clr_shadow.b   = udata.color_shadow.b;
      clr_shadow.a   = udata.color_shadow.a;
      clr_shadow.set = udata.color_shadow.set;

      shadow_type = udata.shadow_type;
      compositing = udata.compositing;
      shaped      = udata.shaped;

      xml_stack_free (udata.stack);
    }

  if (custom_theme_alloc_func)
    {
      theme =
	custom_theme_alloc_func (theme_type,
			MBWMObjectPropWm,                  wm,
			MBWMObjectPropThemePath,           path,
			MBWMObjectPropThemeImg,            img,
			MBWMObjectPropThemeXmlClients,     xml_clients,
			MBWMObjectPropThemeColorLowlight, &clr_lowlight,
			MBWMObjectPropThemeColorShadow,   &clr_shadow,
			MBWMObjectPropThemeShadowType,     shadow_type,
			MBWMObjectPropThemeCompositing,    compositing,
			MBWMObjectPropThemeShaped,         shaped,
			NULL);
    }
  else if (theme_type)
    {
      theme =
	MB_WM_THEME (mb_wm_object_new (theme_type,
			MBWMObjectPropWm,                  wm,
			MBWMObjectPropThemePath,           path,
			MBWMObjectPropThemeImg,            img,
			MBWMObjectPropThemeXmlClients,     xml_clients,
			MBWMObjectPropThemeColorLowlight, &clr_lowlight,
			MBWMObjectPropThemeColorShadow,   &clr_shadow,
			MBWMObjectPropThemeShadowType,     shadow_type,
			MBWMObjectPropThemeCompositing,    compositing,
			MBWMObjectPropThemeShaped,         shaped,
			NULL));
    }

 default_theme:

  if (!theme)
    {
      theme = MB_WM_THEME (mb_wm_object_new (
			MB_WM_TYPE_THEME,
	                MBWMObjectPropWm,                  wm,
			MBWMObjectPropThemeXmlClients,     xml_clients,
			MBWMObjectPropThemeImg,            img,
			MBWMObjectPropThemeColorLowlight, &clr_lowlight,
			MBWMObjectPropThemeColorShadow,   &clr_shadow,
			MBWMObjectPropThemeShadowType,     shadow_type,
			MBWMObjectPropThemeCompositing,    compositing,
			MBWMObjectPropThemeShaped,         shaped,
			NULL));
    }

  if (par)
    XML_ParserFree (par);

  if (file)
    fclose (file);

  if (img)
    free (img);

  return theme;
}

MBWMDecor *
mb_wm_theme_create_decor (MBWMTheme             *theme,
			  MBWindowManagerClient *client,
			  MBWMDecorType          type)
{
  MBWMThemeClass *klass;

  MBWM_ASSERT (client);

  if (!theme || !client)
    return NULL;

  klass = MB_WM_THEME_CLASS(MB_WM_OBJECT_GET_CLASS (theme));

  if (klass->create_decor)
    return klass->create_decor (theme, client, type);

  return NULL;
}

void
mb_wm_theme_resize_decor (MBWMTheme *theme, MBWMDecor *decor)
{
  MBWMThemeClass *klass;

  MBWM_ASSERT (decor);

  if (!theme || !decor)
    return;

  klass = MB_WM_THEME_CLASS(MB_WM_OBJECT_GET_CLASS (theme));

  if (klass->resize_decor)
    klass->resize_decor (theme, decor);
}

void
mb_wm_theme_set_left_padding (MBWMTheme *theme,
                              MBWindowManagerClient *client, int new_padding)
{
  MBWMThemeClass *klass = MB_WM_THEME_CLASS(MB_WM_OBJECT_GET_CLASS (theme));
  MBWMClientType c_type = MB_WM_CLIENT_CLIENT_TYPE (client);
  MBWMDecor *decor = NULL, *decor_tmp;
 
  decor_tmp = (MBWMDecor *)mb_wm_xml_client_find_by_type (theme->xml_clients,
                                                          c_type);
  if (decor_tmp)
    decor = (MBWMDecor *)mb_wm_xml_decor_find_by_type (decor_tmp->decors,
                                                       MBWMDecorTypeNorth);

  if (decor && klass->set_left_padding)
    klass->set_left_padding (theme, decor, new_padding);
}

MBWMClientLayoutHints
mb_wm_theme_get_client_layout_hints (MBWMTheme             * theme,
				     MBWindowManagerClient * client)
{
  MBWMXmlClient * c;
  MBWMClientType  c_type;

  if (!client || !theme)
    return 0;

  c_type = MB_WM_CLIENT_CLIENT_TYPE (client);

  if (!theme->xml_clients ||
      !(c = mb_wm_xml_client_find_by_type (theme->xml_clients, c_type)))
    {
      return 0;
    }

  return c->layout_hints;
}

/**
 * Finds the geometry prescribed for the given client.
 *
 * @return True if the theme prescribes at least one value for the geometry.
 */
Bool
mb_wm_theme_get_client_geometry (MBWMTheme             * theme,
				 MBWindowManagerClient * client,
				 MBGeometry            * geom)
{
  MBWMXmlClient * c;
  MBWMClientType  c_type;

  if (!geom || !client || !theme)
    return False;

  c_type = MB_WM_CLIENT_CLIENT_TYPE (client);

  if (!theme || !theme->xml_clients ||
      !(c = mb_wm_xml_client_find_by_type (theme->xml_clients, c_type)) ||
      (c->client_x < 0 && c->client_y < 0 && c->client_width < 0 && c->client_height < 0))
    {
      return False;
    }

  geom->x      = c->client_x;
  geom->y      = c->client_y;
  geom->width  = c->client_width;
  geom->height = c->client_height;

  return True;
}

Bool
mb_wm_theme_is_client_shaped (MBWMTheme             * theme,
			      MBWindowManagerClient * client)
{
#ifdef HAVE_XEXT
  MBWMXmlClient * c;
  MBWMClientType  c_type;

  if (!client || !theme || !theme->shaped || client->is_argb32)
    return False;

  c_type = MB_WM_CLIENT_CLIENT_TYPE (client);

  if (theme->xml_clients &&
      (c = mb_wm_xml_client_find_by_type (theme->xml_clients, c_type)))
    {
      return c->shaped;
    }

  return False;
#else
  return False;
#endif
}

/*
 * Retrieves color to be used for lowlighting (16-bit rgba)
 */
void
mb_wm_theme_get_lowlight_color (MBWMTheme             * theme,
				unsigned int          * red,
				unsigned int          * green,
				unsigned int          * blue,
				unsigned int          * alpha)
{
  if (theme)
    {
      if (red)
	*red = (unsigned int)(theme->color_lowlight.r * (double)0xffff);

      if (green)
	*green = (unsigned int)(theme->color_lowlight.g * (double)0xffff);

      if (blue)
	*blue = (unsigned int)(theme->color_lowlight.b * (double)0xffff);

      if (alpha)
	*alpha = (unsigned int)(theme->color_lowlight.a * (double)0xffff);

      return;
    }

  if (red)
    *red = 0;

  if (green)
    *green = 0;

  if (blue)
    *blue = 0;

  if (*alpha)
    *alpha = 0x8d8d;
}

/*
 * Retrieves color to be used for shadows (16-bit rgba)
 */
void
mb_wm_theme_get_shadow_color (MBWMTheme             * theme,
			      unsigned int          * red,
			      unsigned int          * green,
			      unsigned int          * blue,
			      unsigned int          * alpha)
{
  if (theme)
    {
      if (red)
	*red = (unsigned int)(theme->color_shadow.r * (double)0xffff);

      if (green)
	*green = (unsigned int)(theme->color_shadow.g * (double)0xffff);

      if (blue)
	*blue = (unsigned int)(theme->color_shadow.b * (double)0xffff);

      if (alpha)
	*alpha = (unsigned int)(theme->color_shadow.a * (double)0xffff);

      return;
    }

  if (red)
    *red = 0;

  if (green)
    *green = 0;

  if (blue)
    *blue = 0;

  if (*alpha)
    *alpha = 0xff00;
}

MBWMCompMgrShadowType
mb_wm_theme_get_shadow_type (MBWMTheme * theme)
{
  if (!theme)
    return MBWM_COMP_MGR_SHADOW_NONE;

  return theme->shadow_type;
}

Bool
mb_wm_theme_use_compositing_mgr (MBWMTheme * theme)
{
  if (!theme)
    return False;

  return theme->compositing;
}

/*
 * Expat callback stuff
 */

static void
xml_stack_push (MBWMList ** stack, XmlCtx ctx)
{
  struct stack_data * s = malloc (sizeof (struct stack_data));

  s->ctx = ctx;
  s->data = NULL;

  *stack = mb_wm_util_list_prepend (*stack, s);
}

static XmlCtx
xml_stack_top_ctx (MBWMList *stack)
{
  struct stack_data * s = stack->data;

  return s->ctx;
}

static void *
xml_stack_top_data (MBWMList *stack)
{
  struct stack_data * s = stack->data;

  return s->data;
}

static void
xml_stack_top_set_data (MBWMList *stack, void * data)
{
  struct stack_data * s = stack->data;

  s->data = data;
}

static void
xml_stack_pop (MBWMList ** stack)
{
  MBWMList * top = *stack;
  struct stack_data * s = top->data;

  *stack = top->next;
  free (s);
  free (top);
}

static void
xml_stack_free (MBWMList *stack)
{
  MBWMList * l = stack;
  while (l)
    {
      MBWMList * n = l->next;
      free (l->data);
      free (l);

      l = n;
    }
}

static void
xml_element_start_cb (void *data, const char *tag, const char **expat_attr)
{
  struct expat_data * exd = data;

  MBWM_DBG ("tag <%s>\n", tag);

  if (!strcmp (tag, "theme"))
    {
      MBWMColor clr;
      const char ** p = expat_attr;

      xml_stack_push (&exd->stack, XML_CTX_THEME);

      while (*p)
	{
	  if (!strcmp (*p, "engine-version"))
	    exd->version = atoi (*(p+1));
	  else if (!strcmp (*p, "engine-type"))
	    {
	      if (!strcmp (*(p+1), "default"))
		exd->theme_type = MB_WM_TYPE_THEME;
#if THEME_PNG
	      else if (!strcmp (*(p+1), "png"))
		exd->theme_type = MB_WM_TYPE_THEME_PNG;
#endif
	      else if (custom_theme_type_func)
		exd->theme_type = custom_theme_type_func (*(p+1),
						  custom_theme_type_func_data);
	    }
	  else if (!strcmp (*p, "shaped"))
	    {
	      if (!strcmp (*(p+1), "yes") || !strcmp (*(p+1), "1"))
		exd->shaped = 1;
	    }
	  else if (!strcmp (*p, "color-shadow"))
	    {
	      mb_wm_xml_clr_from_string (&clr, *(p+1));

	      if (clr.set)
		{
		  exd->color_shadow.r = clr.r;
		  exd->color_shadow.g = clr.g;
		  exd->color_shadow.b = clr.b;
		  exd->color_shadow.a = clr.a;
		  exd->color_shadow.set = True;
		}
	    }
	  else if (!strcmp (*p, "color-lowlight"))
	    {
	      mb_wm_xml_clr_from_string (&clr, *(p+1));

	      if (clr.set)
		{
		  exd->color_lowlight.r = clr.r;
		  exd->color_lowlight.g = clr.g;
		  exd->color_lowlight.b = clr.b;
		  exd->color_lowlight.a = clr.a;
		  exd->color_lowlight.set = True;
		}
	    }
	  else if (!strcmp (*p, "shadow-type"))
	    {
	      if (!strcmp (*(p+1), "simple"))
		exd->shadow_type = MBWM_COMP_MGR_SHADOW_SIMPLE;
	      else if (!strcmp (*(p+1), "gaussian"))
		exd->shadow_type = MBWM_COMP_MGR_SHADOW_GAUSSIAN;
	    }
	  else if (!strcmp (*p, "compositing"))
	    {
	      if (!strcmp (*(p+1), "yes") || !strcmp (*(p+1), "1"))
		exd->compositing = True;
	      else
		exd->compositing = False;
	    }

	  p += 2;
	}
    }

  if (!strcmp (tag, "client"))
    {
      MBWMXmlClient * c = mb_wm_xml_client_new ();
      const char **p = expat_attr;

      XmlCtx ctx = xml_stack_top_ctx (exd->stack);

      xml_stack_push (&exd->stack, XML_CTX_CLIENT);

      if (ctx != XML_CTX_THEME)
	{
	  MBWM_DBG ("Expected context theme");
          free (c);
	  return;
	}

      while (*p)
	{
	  if (!strcmp (*p, "type"))
	    {
	      if (!strcmp (*(p+1), "app"))
		c->type = MBWMClientTypeApp;
	      else if (!strcmp (*(p+1), "dialog"))
		c->type = MBWMClientTypeDialog;
	      else if (!strcmp (*(p+1), "panel"))
		c->type = MBWMClientTypePanel;
	      else if (!strcmp (*(p+1), "input"))
		c->type = MBWMClientTypeInput;
	      else if (!strcmp (*(p+1), "desktop"))
		c->type = MBWMClientTypeDesktop;
	      else if (!strcmp (*(p+1), "notification"))
		c->type = MBWMClientTypeNote;
	      else if (custom_client_type_func)
		c->type = custom_client_type_func (*(p+1),
					custom_client_type_func_data);
	    }
	  else if (!strcmp (*p, "shaped"))
	    {
	      if (!strcmp (*(p+1), "yes") || !strcmp (*(p+1), "1"))
		c->shaped = 1;
	    }
	  else if (!strcmp (*p, "width"))
	    {
	      c->client_width = atoi (*(p+1));
	    }
	  else if (!strcmp (*p, "height"))
	    {
	      c->client_height = atoi (*(p+1));
	    }
	  else if (!strcmp (*p, "x"))
	    {
	      c->client_x = atoi (*(p+1));
	    }
	  else if (!strcmp (*p, "y"))
	    {
	      c->client_y = atoi (*(p+1));
	    }
	  else if (!strcmp (*p, "layout-hints") && *(p+1))
	    {
	      /* comma-separate list of hints */
	      char * duph = strdup (*(p+1));
	      char * comma;
	      char * h = duph;

	      while (h)
		{
		  comma = strchr (h, ',');

		  if (comma)
		    *comma = 0;

		  if (!strcmp (h, "reserve-edge-north"))
		    {
		      c->layout_hints |= LayoutPrefReserveEdgeNorth;
		    }
		  else if (!strcmp (h, "reserve-edge-south"))
		    {
		      c->layout_hints |= LayoutPrefReserveEdgeSouth;
		    }
		  else if (!strcmp (h, "reserve-edge-west"))
		    {
		      c->layout_hints |= LayoutPrefReserveEdgeWest;
		    }
		  else if (!strcmp (h, "reserve-edge-east"))
		    {
		      c->layout_hints |= LayoutPrefReserveEdgeEast;
		    }
		  if (!strcmp (h, "reserve-north"))
		    {
		      c->layout_hints |= LayoutPrefReserveNorth;
		    }
		  else if (!strcmp (h, "reserve-south"))
		    {
		      c->layout_hints |= LayoutPrefReserveSouth;
		    }
		  else if (!strcmp (h, "reserve-west"))
		    {
		      c->layout_hints |= LayoutPrefReserveWest;
		    }
		  else if (!strcmp (h, "reserve-east"))
		    {
		      c->layout_hints |= LayoutPrefReserveEast;
		    }
		  else if (!strcmp (h, "grow"))
		    {
		      c->layout_hints |= LayoutPrefGrowToFreeSpace;
		    }
		  else if (!strcmp (h, "free"))
		    {
		      c->layout_hints |= LayoutPrefPositionFree;
		    }
		  else if (!strcmp (h, "full-screen"))
		    {
		      c->layout_hints |= LayoutPrefFullscreen;
		    }
		  else if (!strcmp (h, "fixed-x"))
		    {
		      c->layout_hints |= LayoutPrefFixedX;
		    }
		  else if (!strcmp (h, "fixed-y"))
		    {
		      c->layout_hints |= LayoutPrefFixedY;
		    }
		  else if (!strcmp (h, "overlaps"))
		    {
		      c->layout_hints |= LayoutPrefOverlaps;
		    }

		  if (comma)
		    h = comma + 1;
		  else
		    break;
		}

	      free (duph);
	    }

	  p += 2;
	}

      if (!c->type)
	mb_wm_xml_client_free (c);
      else
	{
	  exd->xml_clients = mb_wm_util_list_prepend (exd->xml_clients, c);
	  xml_stack_top_set_data (exd->stack, c);
	}


      return;
    }

  if (!strcmp (tag, "decor"))
    {
      MBWMXmlDecor * d = mb_wm_xml_decor_new ();
      const char **p = expat_attr;
      XmlCtx ctx = xml_stack_top_ctx (exd->stack);
      MBWMXmlClient * c = xml_stack_top_data (exd->stack);

      xml_stack_push (&exd->stack, XML_CTX_DECOR);

      if (ctx != XML_CTX_CLIENT || !c)
	{
	  MBWM_DBG ("Expected context client");
          free (d);
	  return;
	}

      while (*p)
	{
	  if (!strcmp (*p, "color-fg"))
	    mb_wm_xml_clr_from_string (&d->clr_fg, *(p+1));
	  else if (!strcmp (*p, "color-bg"))
	    mb_wm_xml_clr_from_string (&d->clr_bg, *(p+1));
	  else if (!strcmp (*p, "type"))
	    {
	      if (!strcmp (*(p+1), "north"))
		d->type = MBWMDecorTypeNorth;
	      else if (!strcmp (*(p+1), "south"))
		d->type = MBWMDecorTypeSouth;
	      else if (!strcmp (*(p+1), "east"))
		d->type = MBWMDecorTypeEast;
	      else if (!strcmp (*(p+1), "west"))
		d->type = MBWMDecorTypeWest;
	    }
	  else if (!strcmp (*p, "template-width"))
	    {
	      d->width = atoi (*(p+1));
	    }
	  else if (!strcmp (*p, "template-height"))
	    {
	      d->height = atoi (*(p+1));
	    }
	  else if (!strcmp (*p, "template-x"))
	    {
	      d->x = atoi (*(p+1));
	    }
	  else if (!strcmp (*p, "template-y"))
	    {
	      d->y = atoi (*(p+1));
	    }
	  else if (!strcmp (*p, "template-pad-offset"))
	    {
	      d->pad_offset = atoi (*(p+1));
	    }
	  else if (!strcmp (*p, "template-pad-length"))
	    {
	      d->pad_length = atoi (*(p+1));
	    }
	  else if (!strcmp (*p, "font-size"))
	    {
	      char * end_size = NULL;

	      d->font_units = MBWMXmlFontUnitsPixels;
	      d->font_size = strtol (*(p+1), &end_size, 0);

	      if (end_size && *end_size)
		{
		  if (*end_size == 'p')
		    {
		      switch (*(end_size+1))
			{
			case 't':
			  d->font_units = MBWMXmlFontUnitsPoints;
			  break;
			case 'x':
			default:
			  ;
			}
		    }
		}
	    }
	  else if (!strcmp (*p, "font-family"))
	    {
	      d->font_family = strdup (*(p+1));
	    }
	  else if (!strcmp (*p, "show-title"))
	    {
	      if (!strcmp (*(p+1), "yes") || !strcmp (*(p+1), "1"))
		d->show_title = 1;
	    }

	  p += 2;
	}

      if (!d->type)
	mb_wm_xml_decor_free (d);
      else
	{
	  c->decors = mb_wm_util_list_prepend (c->decors, d);
	  xml_stack_top_set_data (exd->stack, d);
	}

      return;
    }

  if (!strcmp (tag, "button"))
    {
      MBWMXmlButton * b = mb_wm_xml_button_new ();
      const char **p = expat_attr;
      XmlCtx ctx = xml_stack_top_ctx (exd->stack);
      MBWMXmlDecor * d = xml_stack_top_data (exd->stack);

      xml_stack_push (&exd->stack, XML_CTX_BUTTON);

      if (ctx != XML_CTX_DECOR || !d)
	{
	  MBWM_DBG ("Expected context decor");
          free (b);
	  return;
	}

      while (*p)
	{
	  if (!strcmp (*p, "color-fg"))
	    mb_wm_xml_clr_from_string (&b->clr_fg, *(p+1));
	  else if (!strcmp (*p, "color-bg"))
	    mb_wm_xml_clr_from_string (&b->clr_bg, *(p+1));
	  else if (!strcmp (*p, "type"))
	    {
	      if (!strcmp (*(p+1), "minimize"))
		b->type = MBWMDecorButtonMinimize;
	      else if (!strcmp (*(p+1), "close"))
		b->type = MBWMDecorButtonClose;
	      else if (!strcmp (*(p+1), "menu"))
		b->type = MBWMDecorButtonMenu;
	      else if (!strcmp (*(p+1), "accept"))
		b->type = MBWMDecorButtonAccept;
	      else if (!strcmp (*(p+1), "fullscreen"))
		b->type = MBWMDecorButtonFullscreen;
	      else if (!strcmp (*(p+1), "help"))
		b->type = MBWMDecorButtonHelp;
	      else if (custom_button_type_func)
		b->type = custom_button_type_func (*(p+1),
						custom_button_type_func_data);
	    }
	  else if (!strcmp (*p, "packing"))
	    {
	      if (!strcmp (*(p+1), "end"))
		b->packing = MBWMDecorButtonPackEnd;
	      else if (!strcmp (*(p+1), "start"))
		b->packing = MBWMDecorButtonPackStart;
	    }
	  else if (!strcmp (*p, "template-x"))
	    {
	      b->x = atoi (*(p+1));
	    }
	  else if (!strcmp (*p, "template-y"))
	    {
	      b->y = atoi (*(p+1));
	    }
	  else if (!strcmp (*p, "width"))
	    {
	      b->width = atoi (*(p+1));
	    }
	  else if (!strcmp (*p, "height"))
	    {
	      b->height = atoi (*(p+1));
	    }
	  else if (!strcmp (*p, "template-active-x"))
	    {
	      b->active_x = atoi (*(p+1));
	    }
	  else if (!strcmp (*p, "template-active-y"))
	    {
	      b->active_y = atoi (*(p+1));
	    }
	  else if (!strcmp (*p, "template-inactive-x"))
	    {
	      b->inactive_x = atoi (*(p+1));
	    }
	  else if (!strcmp (*p, "template-inactive-y"))
	    {
	      b->inactive_y = atoi (*(p+1));
	    }
	  else if (!strcmp (*p, "press-activated"))
	    {
	      if (!strcmp (*(p+1), "yes") || !strcmp (*(p+1), "1"))
		b->press_activated = 1;
	    }

	  p += 2;
	}

      if (!b->type)
	{
	  mb_wm_xml_button_free (b);
	  return;
	}

      d->buttons = mb_wm_util_list_append (d->buttons, b);

      xml_stack_top_set_data (exd->stack, b);

      return;
    }

  if (!strcmp (tag, "img"))
    {
      const char **p = expat_attr;
      XmlCtx ctx = xml_stack_top_ctx (exd->stack);

      xml_stack_push (&exd->stack, XML_CTX_IMG);

      if (ctx != XML_CTX_THEME)
	{
	  MBWM_DBG ("Expected context theme");
	  return;
	}

      while (*p)
	{
	  if (!strcmp (*p, "src"))
	    {
	      exd->img = strdup (*(p+1));
	      return;
	    }

	  p += 2;
	}

      return;
    }

}

static void
xml_element_end_cb (void *data, const char *tag)
{
  struct expat_data * exd = data;

  XmlCtx ctx = xml_stack_top_ctx (exd->stack);

  MBWM_DBG ("tag </%s>\n", tag);

  if (!strcmp (tag, "theme"))
    {
      XML_StopParser (exd->par, 0);
    }
  else if (!strcmp (tag, "client"))
    {
      if (ctx == XML_CTX_CLIENT)
	{
	  xml_stack_pop (&exd->stack);
	}
      else
	MBWM_DBG ("Expected client on the top of the stack!");
    }
  else if (!strcmp (tag, "decor"))
    {
      if (ctx == XML_CTX_DECOR)
	{
	  xml_stack_pop (&exd->stack);
	}
      else
	MBWM_DBG ("Expected decor on the top of the stack!");
    }
  else if (!strcmp (tag, "button"))
    {
      if (ctx == XML_CTX_BUTTON)
	{
	  xml_stack_pop (&exd->stack);
	}
      else
	MBWM_DBG ("Expected button on the top of the stack!");
    }
  else if (!strcmp (tag, "img"))
    {
      if (ctx == XML_CTX_IMG)
	{
	  xml_stack_pop (&exd->stack);
	}
      else
	MBWM_DBG ("Expected img on the top of the stack!");
    }
}



static void
construct_buttons (MBWMTheme * theme,
		   MBWMDecor * decor, MBWMXmlDecor *d)
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

      return;
    }

  button = mb_wm_decor_button_stock_new (wm,
					 MBWMDecorButtonClose,
					 MBWMDecorButtonPackEnd,
					 decor,
					 0);

  mb_wm_decor_button_show (button);
  mb_wm_object_unref (MB_WM_OBJECT (button));

#if 0
  /*
   * We probably do not want this in the default client, but for now
   * it is useful for testing purposes
   */
  button = mb_wm_decor_button_stock_new (wm,
					 MBWMDecorButtonFullscreen,
					 MBWMDecorButtonPackEnd,
					 decor,
					 0);

  mb_wm_decor_button_show (button);
  mb_wm_object_unref (MB_WM_OBJECT (button));

  button = mb_wm_decor_button_stock_new (wm,
					 MBWMDecorButtonHelp,
					 MBWMDecorButtonPackEnd,
					 decor,
					 0);

  mb_wm_decor_button_show (button);
  mb_wm_object_unref (MB_WM_OBJECT (button));

  button = mb_wm_decor_button_stock_new (wm,
					 MBWMDecorButtonAccept,
					 MBWMDecorButtonPackEnd,
					 decor,
					 0);

  mb_wm_decor_button_show (button);
  mb_wm_object_unref (MB_WM_OBJECT (button));

  button = mb_wm_decor_button_stock_new (wm,
					 MBWMDecorButtonMinimize,
					 MBWMDecorButtonPackEnd,
					 decor,
					 0);

  mb_wm_decor_button_show (button);
  mb_wm_object_unref (MB_WM_OBJECT (button));

  button = mb_wm_decor_button_stock_new (wm,
					 MBWMDecorButtonMenu,
					 MBWMDecorButtonPackStart,
					 decor,
					 0);

  mb_wm_decor_button_show (button);
  mb_wm_object_unref (MB_WM_OBJECT (button));
#endif
}

struct DecorData
{
  Pixmap            xpix;
  XftDraw          *xftdraw;
  XftColor          clr;
  XftFont          *font;
};

static void
decordata_free (MBWMDecor * decor, void *data)
{
  struct DecorData * dd = data;
  Display * xdpy = decor->parent_client->wmref->xdpy;

  XFreePixmap (xdpy, dd->xpix);

  XftDrawDestroy (dd->xftdraw);

  if (dd->font)
    XftFontClose (xdpy, dd->font);

  free (dd);
}

static MBWMDecor *
mb_wm_theme_simple_create_decor (MBWMTheme             *theme,
				 MBWindowManagerClient *client,
				 MBWMDecorType          type)
{
  MBWMClientType   c_type = MB_WM_CLIENT_CLIENT_TYPE (client);
  MBWMDecor       *decor = NULL;
  MBWindowManager *wm = client->wmref;
  MBWMXmlClient   *c;

  if (MB_WM_THEME (theme)->xml_clients &&
      (c = mb_wm_xml_client_find_by_type (MB_WM_THEME (theme)->xml_clients,
					  c_type)))
    {
      MBWMXmlDecor *d;

      d = mb_wm_xml_decor_find_by_type (c->decors, type);

      if (d)
	{
	  decor = mb_wm_decor_new (wm, type);
	  mb_wm_decor_attach (decor, client);
	  construct_buttons (theme, decor, d);
	}

      return decor;
    }

  switch (c_type)
    {
    case MBWMClientTypeApp:
      switch (type)
	{
	case MBWMDecorTypeNorth:
	  decor = mb_wm_decor_new (wm, type);
	  mb_wm_decor_attach (decor, client);
	  construct_buttons (theme, decor, NULL);
	  break;
	default:
	  decor = mb_wm_decor_new (wm, type);
	  mb_wm_decor_attach (decor, client);
	}
      break;

    case MBWMClientTypeDialog:
      decor = mb_wm_decor_new (wm, type);
      mb_wm_decor_attach (decor, client);
      break;

    case MBWMClientTypePanel:
    case MBWMClientTypeDesktop:
    case MBWMClientTypeInput:
    default:
	  decor = mb_wm_decor_new (wm, type);
	  mb_wm_decor_attach (decor, client);
    }

  return decor;
}

static void
mb_wm_theme_simple_get_button_size (MBWMTheme             *theme,
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

  /*
   * These are defaults when no theme description was loaded
   */
  switch (c_type)
    {
    case MBWMClientTypeApp:
    case MBWMClientTypeDialog:
    case MBWMClientTypePanel:
    case MBWMClientTypeDesktop:
    case MBWMClientTypeInput:
    default:
      if (width)
	*width = SIMPLE_FRAME_TITLEBAR_HEIGHT-4;

      if (height)
	*height = SIMPLE_FRAME_TITLEBAR_HEIGHT-4;
    }
}

static void
mb_wm_theme_simple_get_button_position (MBWMTheme             *theme,
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
	  if (x) {
	    if (b->x >= 0)
	      *x = b->x;
	    else
	      *x = 2;
          }

	  if (y) {
	    if (b->y >= 0)
	      *y = b->y;
	    else
	      *y = 2;
          }

	  return;
	}
    }

  if (x)
    *x = 2;

  if (y)
    *y = 2;
}

static void
mb_wm_theme_simple_get_decor_dimensions (MBWMTheme             *theme,
					 MBWindowManagerClient *client,
					 int                   *north,
					 int                   *south,
					 int                   *west,
					 int                   *east)
{
  MBWMClientType c_type = MB_WM_CLIENT_CLIENT_TYPE (client);
  MBWMXmlClient * c;
  MBWMXmlDecor  * d;

  /*
   * Returning all 0 if in the full screen mode.
   */
  if (mb_wm_client_window_is_state_set (
	client->window,
	MBWMClientWindowEWMHStateFullscreen))
  {
    if (north)
      *north = 0;
    if (south)
      *south = 0;
    if (west)
      *west = 0;
    if (east)
      *east = 0;
    return;
  }

  if ((c = mb_wm_xml_client_find_by_type (theme->xml_clients, c_type)))
    {
      if (north) {
	if ((d = mb_wm_xml_decor_find_by_type (c->decors,MBWMDecorTypeNorth)))
	  *north = d->height;
	else
	  *north = SIMPLE_FRAME_TITLEBAR_HEIGHT;
      }

      if (south) {
	if ((d = mb_wm_xml_decor_find_by_type (c->decors,MBWMDecorTypeSouth)))
	  *south = d->height;
	else
	  *south = SIMPLE_FRAME_EDGE_SIZE;
      }

      if (west) {
	if ((d = mb_wm_xml_decor_find_by_type (c->decors, MBWMDecorTypeWest)))
	  *west = d->width;
	else
	  *west = SIMPLE_FRAME_EDGE_SIZE;
      }

      if (east) {
	if ((d = mb_wm_xml_decor_find_by_type (c->decors, MBWMDecorTypeEast)))
	  *east = d->width;
	else
	  *east = SIMPLE_FRAME_EDGE_SIZE;
      }

      return;
    }

  /*
   * These are defaults when no theme description was loaded
   */
  switch (c_type)
    {
    case MBWMClientTypeDialog:
    case MBWMClientTypeApp:
      if (north)
	*north = SIMPLE_FRAME_TITLEBAR_HEIGHT;

      if (south)
	*south = SIMPLE_FRAME_EDGE_SIZE;

      if (west)
	*west = SIMPLE_FRAME_EDGE_SIZE;

      if (east)
	*east = SIMPLE_FRAME_EDGE_SIZE;
      break;

    case MBWMClientTypePanel:
    case MBWMClientTypeDesktop:
    case MBWMClientTypeInput:
    default:
      if (north)
	*north = 0;

      if (south)
	*south = 0;

      if (west)
	*west = 0;

      if (east)
	*east = 0;
    }
}

static unsigned long
pixel_from_clr (Display * dpy, int screen, MBWMColor * clr)
{
  XColor xcol;

  xcol.red   = (int)(clr->r * (double)0xffff);
  xcol.green = (int)(clr->g * (double)0xffff);
  xcol.blue  = (int)(clr->b * (double)0xffff);
  xcol.flags = DoRed|DoGreen|DoBlue;

  XAllocColor (dpy, DefaultColormap (dpy, screen), &xcol);

  return xcol.pixel;
}

static XftFont *
xft_load_font (MBWMDecor * decor, MBWMXmlDecor *d)
{
  char      desc[512];
  XftFont * font;
  Display * xdpy = decor->parent_client->wmref->xdpy;
  int       xscreen = decor->parent_client->wmref->xscreen;
  int       font_size;

  font_size = d && d->font_size ? d->font_size : SIMPLE_FRAME_TITLEBAR_HEIGHT / 2;

  if (!d || d->font_units == MBWMXmlFontUnitsPixels)
    {
      font_size = mb_wm_util_pixels_to_points (decor->parent_client->wmref,
					       font_size);
    }

  snprintf (desc, sizeof (desc), "%s-%i",
	    d && d->font_family ? d->font_family : "Sans",
	    font_size);

  font = XftFontOpenName (xdpy, xscreen, desc);

  return font;
}

static void
mb_wm_theme_simple_paint_decor (MBWMTheme *theme, MBWMDecor *decor)
{
  MBWMDecorType          type;
  const MBGeometry      *geom;
  MBWindowManagerClient *client;
  Window                 xwin;
  MBWindowManager       *wm = theme->wm;
  MBWMColor              clr_bg;
  MBWMColor              clr_fg;
  MBWMClientType         c_type;
  MBWMXmlClient         *c = NULL;
  MBWMXmlDecor          *d = NULL;
  struct DecorData      *dd;
  int x, y, w, h;
  GC                     gc;
  Display               *xdpy = wm->xdpy;
  int                    xscreen = wm->xscreen;
  const char            *title;

  clr_fg.r = 1.0;
  clr_fg.g = 1.0;
  clr_fg.b = 1.0;

  clr_bg.r = 0.5;
  clr_bg.g = 0.5;
  clr_bg.b = 0.5;

  client = mb_wm_decor_get_parent (decor);
  xwin = mb_wm_decor_get_x_window (decor);

  if (client == NULL || xwin == None)
    return;

  dd = mb_wm_decor_get_theme_data (decor);

  type   = mb_wm_decor_get_type (decor);
  geom   = mb_wm_decor_get_geometry (decor);
  c_type = MB_WM_CLIENT_CLIENT_TYPE (client);

  if ((c = mb_wm_xml_client_find_by_type (theme->xml_clients, c_type)) &&
      (d = mb_wm_xml_decor_find_by_type (c->decors, decor->type)))
    {
      if (d->clr_fg.set)
	{
	  clr_fg.r = d->clr_fg.r;
	  clr_fg.g = d->clr_fg.g;
	  clr_fg.b = d->clr_fg.b;
	}

      if (d->clr_bg.set)
	{
	  clr_bg.r = d->clr_bg.r;
	  clr_bg.g = d->clr_bg.g;
	  clr_bg.b = d->clr_bg.b;
	}
    }

  if (!dd)
    {
      XRenderColor rclr;

      dd = malloc (sizeof (struct DecorData));
      dd->xpix = XCreatePixmap(xdpy, xwin,
			       decor->geom.width, decor->geom.height,
			       DefaultDepth(xdpy, xscreen));

      dd->xftdraw = XftDrawCreate (xdpy, dd->xpix,
				   DefaultVisual (xdpy, xscreen),
				   DefaultColormap (xdpy, xscreen));

      rclr.red   = (int)(clr_fg.r * (double)0xffff);
      rclr.green = (int)(clr_fg.g * (double)0xffff);
      rclr.blue  = (int)(clr_fg.b * (double)0xffff);
      rclr.alpha = 0xffff;

      XftColorAllocValue (xdpy, DefaultVisual (xdpy, xscreen),
			  DefaultColormap (xdpy, xscreen),
			  &rclr, &dd->clr);

      dd->font = xft_load_font (decor, d);

      XSetWindowBackgroundPixmap(xdpy, xwin, dd->xpix);

      mb_wm_decor_set_theme_data (decor, dd, decordata_free);
    }

  gc = XCreateGC (xdpy, dd->xpix, 0, NULL);

  XSetLineAttributes (xdpy, gc, 1, LineSolid, CapProjecting, JoinMiter);
  XSetBackground (xdpy, gc, pixel_from_clr (xdpy, xscreen, &clr_bg));
  XSetForeground (xdpy, gc, pixel_from_clr (xdpy, xscreen, &clr_bg));

  w = geom->width; h = geom->height; x = geom->x; y = geom->y;

  XFillRectangle (xdpy, dd->xpix, gc, 0, 0, w, h);

  if (mb_wm_decor_get_type(decor) == MBWMDecorTypeNorth &&
      (title = mb_wm_client_get_name (client)))
    {
      XRectangle rec;
      int centering_padding = 0;
      int is_secondary_dialog = mb_window_is_secondary (wm, xwin);
      int pack_start_x = mb_wm_decor_get_pack_start_x (decor);
      int pack_end_x = mb_wm_decor_get_pack_end_x (decor);
      int west_width = mb_wm_client_frame_west_width (client);
      int y = (decor->geom.height -
	       (dd->font->ascent + dd->font->descent)) / 2
	+ dd->font->ascent;

      rec.x = left_padding;
      rec.y = 0;
      rec.width = pack_end_x - 2;
      rec.height = d ? d->height : SIMPLE_FRAME_TITLEBAR_HEIGHT;

      if (is_secondary_dialog)
        {
	  XGlyphInfo extents;

	  XftTextExtentsUtf8 (xdpy,
			  dd->font,
			  title, strlen(title),
			  &extents);
	  centering_padding = (rec.width - extents.width) / 2;
      }

      XftDrawSetClipRectangles (dd->xftdraw, 0, 0, &rec, 1);

      XftDrawStringUtf8(dd->xftdraw,
			&dd->clr,
			dd->font,
			left_padding + centering_padding + west_width + pack_start_x + (h / 5), y,
			title, strlen (title));
    }

  XFreeGC (xdpy, gc);

  XClearWindow (xdpy, xwin);
}

static void
mb_wm_theme_simple_paint_button (MBWMTheme *theme, MBWMDecorButton *button)
{
  MBWMDecor             *decor;
  MBWindowManagerClient *client;
  Window                 xwin;
  MBWindowManager       *wm = theme->wm;
  int                    x, y, w, h;
  MBWMColor              clr_bg;
  MBWMColor              clr_fg;
  MBWMClientType         c_type;
  MBWMXmlClient         *c = NULL;
  MBWMXmlDecor          *d = NULL;
  MBWMXmlButton         *b = NULL;
  struct DecorData * dd;
  GC                     gc;
  Display               *xdpy = wm->xdpy;
  int                    xscreen = wm->xscreen;

  clr_fg.r = 1.0;
  clr_fg.g = 1.0;
  clr_fg.b = 1.0;

  clr_bg.r = 0.0;
  clr_bg.g = 0.0;
  clr_bg.b = 0.0;

  decor = button->decor;
  client = mb_wm_decor_get_parent (decor);
  xwin = decor->xwin;
  dd = mb_wm_decor_get_theme_data (decor);

  if (client == NULL || xwin == None || dd->xpix == None)
    return;

  c_type = MB_WM_CLIENT_CLIENT_TYPE (client);

  if ((c = mb_wm_xml_client_find_by_type (theme->xml_clients, c_type)) &&
      (d = mb_wm_xml_decor_find_by_type (c->decors, decor->type)) &&
      (b = mb_wm_xml_button_find_by_type (d->buttons, button->type)))
    {
      clr_fg.r = b->clr_fg.r;
      clr_fg.g = b->clr_fg.g;
      clr_fg.b = b->clr_fg.b;

      clr_bg.r = b->clr_bg.r;
      clr_bg.g = b->clr_bg.g;
      clr_bg.b = b->clr_bg.b;
    }

  w = button->geom.width;
  h = button->geom.height;
  x = button->geom.x;
  y = button->geom.y;

  gc = XCreateGC (xdpy, dd->xpix, 0, NULL);

  XSetLineAttributes (xdpy, gc, 1, LineSolid, CapRound, JoinRound);



  if (button->state == MBWMDecorButtonStateInactive)
    {
      XSetForeground (xdpy, gc, pixel_from_clr (xdpy, xscreen, &clr_bg));
    }
  else
    {
      /* FIXME -- think of a better way of doing this */
      MBWMColor clr;
      clr.r = clr_bg.r + 0.2;
      clr.g = clr_bg.g + 0.2;
      clr.b = clr_bg.b + 0.2;

      XSetForeground (xdpy, gc, pixel_from_clr (xdpy, xscreen, &clr));
    }

  XFillRectangle (xdpy, dd->xpix, gc, x, y, w+1, h+1);

  XSetLineAttributes (xdpy, gc, 3, LineSolid, CapRound, JoinRound);
  XSetForeground (xdpy, gc, pixel_from_clr (xdpy, xscreen, &clr_fg));

  if (button->type == MBWMDecorButtonClose)
    {
      XDrawLine (xdpy, dd->xpix, gc, x + 3, y + 3, x + w - 3, y + h - 3);
      XDrawLine (xdpy, dd->xpix, gc, x + 3, y + h - 3, x + w - 3, y + 3);
    }
  else if (button->type == MBWMDecorButtonFullscreen)
    {
      XDrawLine (xdpy, dd->xpix, gc, x + 3, y + 3, x + 3, y + h - 3);
      XDrawLine (xdpy, dd->xpix, gc, x + 3, y + h - 3, x + w - 3, y + h - 3);
      XDrawLine (xdpy, dd->xpix, gc, x + w - 3, y + h - 3, x + w - 3, y + 3);
      XDrawLine (xdpy, dd->xpix, gc, x + w - 3, y + 3, x + 3, y + 3);
    }
  else if (button->type == MBWMDecorButtonMinimize)
    {
      XDrawLine (xdpy, dd->xpix, gc, x + 3, y + h - 5, x + w - 3, y + h - 5);
    }
  else if (button->type == MBWMDecorButtonHelp)
    {
      char desc[512];
      XftFont *font;
      XRenderColor rclr;
      XftColor clr;
      XRectangle rec;

      snprintf (desc, sizeof (desc), "%s-%i:bold",
	    d && d->font_family ? d->font_family : "Sans", h*3/4);

      font = XftFontOpenName (xdpy, xscreen, desc);

      rclr.red   = (int)(clr_fg.r * (double)0xffff);
      rclr.green = (int)(clr_fg.g * (double)0xffff);
      rclr.blue  = (int)(clr_fg.b * (double)0xffff);
      rclr.alpha = 0xffff;

      XftColorAllocValue (xdpy, DefaultVisual (xdpy, xscreen),
			  DefaultColormap (xdpy, xscreen),
			  &rclr, &clr);

      rec.x = x;
      rec.y = y;
      rec.width = w;
      rec.height = h;

      XftDrawSetClipRectangles (dd->xftdraw, 0, 0, &rec, 1);

      XftDrawStringUtf8 (dd->xftdraw, &clr, font,
			 x + 4 + left_padding,
			 y + (h - (font->ascent + font->descent))/2 +
			 font->ascent,
			 "?", 1);

      XftFontClose (xdpy, font);
    }
  else if (button->type == MBWMDecorButtonMenu)
    {
      XSetLineAttributes (xdpy, gc, 3, LineSolid, CapRound, JoinMiter);
      XDrawLine (xdpy, dd->xpix, gc, x + 3, y + 5, x + w/2, y + h - 5);
      XDrawLine (xdpy, dd->xpix, gc, x + w/2, y + h - 5, x + w - 3, y + 5);
    }
  else if (button->type == MBWMDecorButtonAccept)
    {
      XDrawArc (xdpy, dd->xpix, gc, x + 4, y + 4, w - 8, h - 8, 0, 64 * 360);
    }

  XFreeGC (xdpy, gc);

  XClearWindow (wm->xdpy, xwin);
}

/*
 * Installs a global handler that can be used to translate custom client type
 * names to their numerical values.
 *
 * NB: this is not an object function, since we need it before we allocate the
 *     actual MBWMTheme object in the XML parser.
 */
void
mb_wm_theme_set_custom_client_type_func (MBWMThemeCustomClientTypeFunc  func,
					 void                     *user_data)
{
  custom_client_type_func = func;
  custom_client_type_func_data = user_data;
}

/*
 * Installs a global handler that can be used to translate custom theme names
 * to their numerical (MBWMObject) values.
 *
 * NB: this is not an object function, since we need it before we allocate the
 *     actual MBWMTheme object in the XML parser.
 */
void
mb_wm_theme_set_custom_theme_type_func (MBWMThemeCustomThemeTypeFunc  func,
					void                     *user_data)
{
  custom_theme_type_func = func;
  custom_theme_type_func_data = user_data;
}

/*
 * Installs a global handler that can be used to translate custom button names
 * to their numerical values.
 *
 * NB: this is not an object function, since we need it before we allocate the
 *     actual MBWMTheme object in the XML parser.
 */
void
mb_wm_theme_set_custom_button_type_func (MBWMThemeCustomButtonTypeFunc  func,
					 void                     *user_data)
{
  custom_button_type_func = func;
  custom_button_type_func_data = user_data;
}

/*
 * Installs a global handler that can be used to allocate a custom
 * MBWMThemeSubclass.
 *
 * NB: this is not an object function, since we need it before we allocate the
 *     actual MBWMTheme object in the XML parser.
 */
void
mb_wm_theme_set_custom_theme_alloc_func (MBWMThemeCustomThemeAllocFunc  func)
{
  custom_theme_alloc_func = func;
}
