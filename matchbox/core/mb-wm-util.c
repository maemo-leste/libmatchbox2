#include "mb-wm.h"
#include <stdarg.h>

#undef  G_LOG_DOMAIN
#define G_LOG_DOMAIN "libmatchbox"

static int TrappedErrorCode = 0;
static int (*old_error_handler) (Display *, XErrorEvent *);

static int
error_handler(Display     *xdpy,
	      XErrorEvent *error)
{
  TrappedErrorCode = error->error_code;
  g_debug("X error %d, window: 0x%lx, req: %d, minor: %d",
          error->error_code, error->resourceid,
          error->request_code, error->minor_code);
  return 0;
}

void
mb_wm_util_trap_x_errors(void)
{
  /* MBWM_DBG("### X Errors Trapped ###"); */

  TrappedErrorCode  = 0;
  old_error_handler = XSetErrorHandler(error_handler);
}

int
mb_wm_util_untrap_x_errors(void)
{
  /* MBWM_DBG("### X Errors Untrapped (%i) ###", TrappedErrorCode); */

  XSetErrorHandler(old_error_handler);
  return TrappedErrorCode;
}


void*
mb_wm_util_malloc0(int size)
{
  void *p = NULL;

  p = malloc(size);

  if (p == NULL)
    {
      /* hook into some kind of out of memory */
    }
  else
    memset(p, 0, size);

  return p;
}

Bool 				/* FIXME: define, inline ? */
mb_geometry_compare (MBGeometry *g1, MBGeometry *g2)
{
  return (g1->x == g2->x
	  && g1->y == g2->y
	  && g1->width == g2->width
	  && g1->height == g2->height);
}

Bool  /* True if overlaps */
mb_geometry_intersects (MBGeometry *g1, MBGeometry *g2)
{
  if ((g1->x > g2->x + g2->width)  ||
      (g1->y > g2->y + g2->height) ||
      (g2->x > g1->x + g1->width)  ||
      (g2->y > g1->y + g1->height))
    return False;

  return True;
}


void
mb_wm_util_fatal_error (char *msg)
{
  fprintf(stderr, "matchbox-window-manager: *Error*  %s\n", msg);
  exit(1);
}

void
mb_wm_util_warn (const char *format, ...)
{
  va_list ap;
  char    *msg = NULL;

  va_start(ap, format);
  vasprintf(&msg, format, ap);
  va_end(ap);

  fprintf(stderr, "*MBWM Warning*  %s\n", msg);

  if (msg) free(msg);
}

MBWMList*
mb_wm_util_list_alloc_item(void)
{
  return mb_wm_util_malloc0(sizeof(MBWMList));
}

int
mb_wm_util_list_length(MBWMList *list)
{
  int result = 1;

  if (!list)
    return 0;

  list = mb_wm_util_list_get_first(list);

  while ((list = mb_wm_util_list_next(list)) != NULL)
    result++;

  return result;
}

MBWMList*
mb_wm_util_list_get_last(MBWMList *list)
{
  if (list == NULL)
    return NULL;

  while (list->next)
    list = mb_wm_util_list_next(list);
  return list;
}

MBWMList*
mb_wm_util_list_get_first(MBWMList *list)
{
  if (list == NULL)
    return NULL;

  while (list->prev)
    list = mb_wm_util_list_prev(list);
  return list;
}

void*
mb_wm_util_list_get_nth_data(MBWMList *list, int n)
{
  if (list == NULL)
    return NULL;

  list = mb_wm_util_list_get_first(list);

  while (list->next && n)
    {
      list = mb_wm_util_list_next(list);
      n--;
    }

  if (n) return NULL;

  return (void *)list->data;
}

MBWMList*
mb_wm_util_list_prepend(MBWMList *list, void *data)
{
  MBWMList * l = mb_wm_util_list_alloc_item();

  l->data = data;
  l->next = list;

  if (list)
    list->prev = l;

  return l;
}

MBWMList*
mb_wm_util_list_append(MBWMList *list, void *data)
{
  if (list == NULL)
    {
      list = mb_wm_util_list_alloc_item();
      list->data = data;
    }
  else
    {
      MBWMList *last;

      last = mb_wm_util_list_get_last(list);

      last->next = mb_wm_util_list_alloc_item();
      last->next->prev = last;
      last->next->data = data;
    }

  return list;
}

MBWMList*
mb_wm_util_list_remove(MBWMList *list, void *data)
{
  MBWMList *prev, *start;

  prev = NULL;
  start = list = mb_wm_util_list_get_first(list);

  while (list)
    {
      if (list->data == data)
	{
	  if (list->next)
	    list->next->prev = prev;

	  if (prev)
	    prev->next = list->next;
	  else
	    start = list->next;

	  free(list);

	  return start;
	}

      prev = list;
      list = list->next;
    }

  return NULL;
}

void
mb_wm_util_list_foreach (const MBWMList   *list,
			 MBWMListForEachCB func,
			 void             *userdata)
{
  MBWMList *p = (MBWMList *) list;

  while (p)
    {
      func(p->data, userdata);
      p = mb_wm_util_list_next(p);
    }
}

void
mb_wm_util_list_free (MBWMList * list)
{
  MBWMList * l = list;

  while (l)
    {
      MBWMList * f = l;
      l = l->next;

      free (f);
    }
}


MBWMRgbaIcon *
mb_wm_rgba_icon_new ()
{
  return mb_wm_util_malloc0 (sizeof (MBWMRgbaIcon));
}


void
mb_wm_rgba_icon_free (MBWMRgbaIcon *icon)
{
  if (icon->pixels)
    free (icon->pixels);

  free (icon);
}

int
mb_wm_util_pixels_to_points (MBWindowManager *wm, int pixels)
{
  static double scale = 0.0; /* Points per pixel */
  int points;

  if (scale == 0.0)
    {
      scale =
	((double)DisplayHeightMM (wm->xdpy, wm->xscreen) * 2.8346456693)
	/ (double) DisplayHeight(wm->xdpy, wm->xscreen);
    }

  /* Scale and round */
  points = (((int)((double)(pixels << 10) * scale) + 512) >> 10);

  return points;
}

