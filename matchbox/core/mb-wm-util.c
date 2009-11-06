#include "mb-wm.h"
#include <stdarg.h>

#include "mb-wm-debug-symbols.h"

#undef  G_LOG_DOMAIN
#define G_LOG_DOMAIN "libmatchbox"

typedef struct {
  const gchar *function_name; /* Unowned - expected to be from __FUNCTION__ */
  gchar *message; /* Owned by this */

  Display *display;
  long serial_start; /* serial number of next request at the time
                        mb_wm_util_async_trap_x_errors was called */
  long serial_end;
} CodeSection;

static GList *code_section_list = 0; /* of CodeSection */

static int TrappedErrorCode = 0;
static int (*old_error_handler) (Display *, XErrorEvent *);

static int
error_handler(Display     *xdpy,
	      XErrorEvent *error)
{
#ifndef G_DEBUG_DISABLE
  char err[64], req[64];

  sprintf(err, "%s (%d)",
          error->error_code < G_N_ELEMENTS (mb_wm_debug_x_errors)
              && mb_wm_debug_x_errors[error->error_code]
            ? mb_wm_debug_x_errors[error->error_code] : "???",
          error->error_code);
  sprintf(req, "%s (%d)",
          error->request_code < G_N_ELEMENTS (mb_wm_debug_x_requests)
              && mb_wm_debug_x_requests[error->request_code]
            ? mb_wm_debug_x_requests[error->request_code] : "???",
          error->request_code);
  g_debug("X error %s, window: 0x%lx, req: %s, minor: %d",
          err, error->resourceid, req, error->minor_code);
#endif

  TrappedErrorCode = error->error_code;

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

/* Called *always* when mb_wm_util_[un]trap_x_errors isn't in place
 * (at least if we called mb_wm_util_async_x_error_init at the start
 * of the program) */
static int
async_error_handler(Display     *xdpy,
                    XErrorEvent *error)
{
  GList *entry;
  gchar error_string[256];
  CodeSection *blamed = 0;

  /* Find the section of code to blame */
  for (entry=code_section_list; entry; entry=entry->next)
    {
      CodeSection *section = (CodeSection *)entry->data;
      if (section->display == error->display &&
          section->serial_start <= error->serial &&
          (!section->serial_end ||
           section->serial_end > error->serial))
        {
          blamed = section;
          break;
        }
    }

  /* If no message was set, it means we don't want it reported */
  if (blamed && !blamed->message)
    return 0;

  /* Error text */
#ifdef G_DEBUG_DISABLE
  sprintf(error_string,
          "X error %d, window: 0x%lx, req: %d, minor: %d",
      error->error_code,
      error->resourceid,
      error->request_code,
      error->minor_code);
#else
  sprintf(error_string,
          "X error %s (%d), window: 0x%lx, req: %s (%d), minor: %d",
      error->error_code < G_N_ELEMENTS (mb_wm_debug_x_errors)
          && mb_wm_debug_x_errors[error->error_code]
        ? mb_wm_debug_x_errors[error->error_code] : "???",
      error->error_code,
      error->resourceid,
      error->request_code < G_N_ELEMENTS (mb_wm_debug_x_requests)
          && mb_wm_debug_x_requests[error->request_code]
        ? mb_wm_debug_x_requests[error->request_code] : "???",
      error->request_code,
      error->minor_code);
#endif //G_DEBUG_DISABLE

  if (blamed)
    g_warning("%s: %s: %s", blamed->function_name?blamed->function_name:"?",
                            blamed->message,
                            error_string);
  else
    g_critical("Untrapped: %s", error_string);
  return 0;
}

/* Install the asynchronous error handler */
void
mb_wm_util_async_x_error_init()
{
  XSetErrorHandler(async_error_handler);
}

/* Remove traps in our list that are older than the most recently
 * processed X event. */
static void
mb_wm_util_async_x_error_free_old()
{
  GList *entry = code_section_list;

  Display *last_display = 0;
  long last_message = 0;

  while (entry)
    {
      GList *next_entry = entry->next;
      CodeSection *section = (CodeSection *)entry->data;
      /* Cache based on display, saving a call to X */
      if (last_display != section->display)
        {
          last_message = LastKnownRequestProcessed(section->display);
          last_display = section->display;
        }
      /* If the serial number is older than the last message,
       * free this item as it isn't required any more
       */
      if (section->serial_end < last_message)
        code_section_list = g_list_remove(code_section_list, section);
      entry = next_entry;
    }
}

/* Add async trap for errors. This means that errors will be handled
 * when they occur and reported to the console, regardless of whether
 * they are in a mb_wm_util_[un]trap_x_errors block.
 *
 * function_name's pointer is used directly and should be static
 * message is copied if it is non-null. If it is null, no error is
 * produced. */
void
mb_wm_util_async_trap_x_errors_full(Display *display,
                                    const gchar *function_name,
                                    const gchar *message)
{
  CodeSection *section = g_malloc(sizeof(CodeSection));

  /* This was purely paranoia */
  /*static int (*old_handler) (Display *, XErrorEvent *);
  old_handler = XSetErrorHandler(async_error_handler);
  if (old_handler != async_error_handler)
    g_warning("mb_wm_util_async_trap_x_errors:"
              " async_error_handler had been overwritten");*/

  section->function_name = function_name;
  if (message)
    section->message = g_strdup(message);
  else
    section->message = 0;
  section->display = display;
  section->serial_start = NextRequest(display);
  section->serial_end = 0;

  if (code_section_list)
    {
      CodeSection *oldsection = (CodeSection *)code_section_list->data;
      if (!oldsection->serial_end)
        {
          oldsection->serial_end = section->serial_start;
          if (function_name)
            g_warning("mb_wm_util_async_trap_x_errors called "
                      "without untrap in %s (found via %s)",
                      oldsection->function_name, function_name);
        }
    }

  code_section_list = g_list_prepend(code_section_list, section);
}

void
mb_wm_util_async_untrap_x_errors_full(const gchar *function_name)
{
  CodeSection *section;
  if (!code_section_list)
    {
      g_warning("mb_wm_util_async_untrap_x_errors called from %s,"
                " but no code_section_list", function_name);
      return;
    }
  section = (CodeSection *)code_section_list->data;
  if (strcmp(section->function_name, function_name))
    {
      g_warning("mb_wm_util_async_untrap_x_errors called "
                "from %s, but trap from %s found",
                function_name, section->function_name);
    }
  section->serial_end = NextRequest(section->display);
  /* Make sure we remove anything in our list that
   * is older than the currently processed X request */
  mb_wm_util_async_x_error_free_old();
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

