#include "mb-wm-main-context.h"

#include <sys/time.h>
#include <poll.h>
#include <limits.h>
#include <fcntl.h>

#if ENABLE_COMPOSITE
#include <X11/extensions/Xdamage.h>
#endif

#define MBWM_CTX_MAX_TIMEOUT 100

#if MBWM_WANT_DEBUG

static const char *MBWMDEBUGEvents[] = {
    "error",
    "reply",
    "KeyPress",
    "KeyRelease",
    "ButtonPress",
    "ButtonRelease",
    "MotionNotify",
    "EnterNotify",
    "LeaveNotify",
    "FocusIn",
    "FocusOut",
    "KeymapNotify",
    "Expose",
    "GraphicsExpose",
    "NoExpose",
    "VisibilityNotify",
    "CreateNotify",
    "DestroyNotify",
    "UnmapNotify",
    "MapNotify",
    "MapRequest",
    "ReparentNotify",
    "ConfigureNotify",
    "ConfigureRequest",
    "GravityNotify",
    "ResizeRequest",
    "CirculateNotify",
    "CirculateRequest",
    "PropertyNotify",
    "SelectionClear",
    "SelectionRequest",
    "SelectionNotify",
    "ColormapNotify",
    "ClientMessage",
    "MappingNotify",
};

#endif

#if ! USE_GLIB_MAINLOOP
static Bool
mb_wm_main_context_check_timeouts (MBWMMainContext *ctx);

static Bool
mb_wm_main_context_check_fd_watches (MBWMMainContext * ctx);
#endif

static Bool
mb_wm_main_context_spin_xevent (MBWMMainContext *ctx);

static void 
mb_wm_list_remove_deleted_handlers (MBWMList **l_start);

static void
mb_wm_main_context_remove_deleted_handlers (MBWMMainContext *ctx);

struct MBWMTimeOutEventInfo
{
  int                         ms;
  MBWindowManagerTimeOutFunc  func;
  void                       *userdata;
  unsigned long               id;
  struct timeval              triggers;

};

struct MBWMFdWatchInfo{
  MBWMIOChannel               *channel;
  MBWMIOCondition             events;
  MBWindowManagerFdWatchFunc  func;
  void                       *userdata;
  unsigned long               id;
};

static void
mb_wm_main_context_class_init (MBWMObjectClass *klass)
{
#if MBWM_WANT_DEBUG
  klass->klass_name = "MBWMMainContext";
#endif
}

static void
mb_wm_main_context_destroy (MBWMObject *this)
{
}

#if USE_GLIB_MAINLOOP
gboolean
mb_wm_main_context_gloop_xevent (gpointer userdata)
{
  MBWMMainContext * ctx = userdata;
  MBWindowManager * wm  = ctx->wm;

  while (mb_wm_main_context_spin_xevent (ctx));

  if (wm->sync_type)
    mb_wm_sync (wm);

  return TRUE;
}
#endif

static int
mb_wm_main_context_init (MBWMObject *this, va_list vap)
{
  MBWMMainContext  *ctx = MB_WM_MAIN_CONTEXT (this);
  MBWindowManager  *wm = NULL;
  MBWMObjectProp    prop;

  prop = va_arg(vap, MBWMObjectProp);
  while (prop)
    {
      if (prop == MBWMObjectPropWm)
	{
	  wm = va_arg(vap, MBWindowManager *);
	  break;
	}
      else
	MBWMO_PROP_EAT (vap, prop);

      prop = va_arg (vap, MBWMObjectProp);
    }

  ctx->wm = wm;

  return 1;
}

int
mb_wm_main_context_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (MBWMMainContextClass),
	sizeof (MBWMMainContext),
	mb_wm_main_context_init,
	mb_wm_main_context_destroy,
	mb_wm_main_context_class_init
      };
      type = mb_wm_object_register_class (&info, MB_WM_TYPE_OBJECT, 0);
    }

  return type;
}

MBWMMainContext*
mb_wm_main_context_new (MBWindowManager *wm)
{
  MBWMMainContext *ctx;

  ctx = MB_WM_MAIN_CONTEXT (mb_wm_object_new (MB_WM_TYPE_MAIN_CONTEXT,
					      MBWMObjectPropWm, wm,
					      NULL));

  return ctx;
}

static inline void
call_handlers_for_event (MBWMList *iter,
			 void *event,
			 Window xwin
			 )
{
  while (iter)
    {
      MBWMXEventFuncInfo *i = iter->data;
      MBWMList        *next = iter->next;

      if (i && !i->deleted && 
		      (i->xwindow == None || i->xwindow == xwin))
	{
	  if (!i->func (event, i->userdata))
	    {
	      /* We don't stop when they ask us to now.
	       * But only warn about this if it's not the last
	       * handler in the chain!
	       */
	      if (next)
		g_debug ("Handler %p asked us to stop.  But we won't.",
                         i->func);
	    }
	}

      iter = next;
    }
}

Bool
mb_wm_main_context_handle_x_event (XEvent          *xev,
				   MBWMMainContext *ctx)
{
  MBWindowManager *wm = ctx->wm;
#if (MBWM_WANT_DEBUG)
  MBWMList        *iter;
  Window           xwin = xev->xany.window;

  if (mbwm_debug_flags & MBWM_DEBUG_EVENT)
    {
      MBWindowManagerClient *ev_client;

      ev_client = mb_wm_managed_client_from_xwindow(wm, xev->xany.window);

      printf ("  @ XEvent: '%s:%i' for %lx %s%s\n",
	      xev->type < sizeof (MBWMDEBUGEvents)/sizeof(MBWMDEBUGEvents[0])
	      ? MBWMDEBUGEvents[xev->type] : "unknown",
	      xev->type,
	      xev->xany.window,
	      xev->xany.window == wm->root_win->xwindow ? "(root)" : "",
	      ev_client ? ev_client->name : ""
	      );

      switch (xev->type)
	{
	case UnmapNotify:
	  {
	    XUnmapEvent * uev = & xev->xunmap;
	    printf ("    window %x, event %x, %d\n",
		    uev->window,
		    uev->event,
		    uev->from_configure);
	  }
	  break;
	case ConfigureNotify:
	  {
	    XConfigureEvent * cev = & xev->xconfigure;
	    printf ("    window %x, event %x, [%d,%d;%dx%d]\n",
		    cev->window,
		    cev->event,
		    cev->x,
		    cev->y,
		    cev->width,
		    cev->height);
	  }
	  break;
	case ConfigureRequest:
	  {
	    XConfigureRequestEvent * cev = & xev->xconfigurerequest;
	    printf ("    window %x, parent %x, [%d,%d;%dx%d]\n",
		    cev->window,
		    cev->parent,
		    cev->x,
		    cev->y,
		    cev->width,
		    cev->height);
	  }
	  break;
	case PropertyNotify:
	  {
	    XPropertyEvent * pev = & xev->xproperty;
	    char * prop = XGetAtomName (wm->xdpy, pev->atom);
	    printf ("    window %x, prop %s, state %d\n",
		    pev->window,
		    prop,
		    pev->state);

	    if (prop)
	      XFree (prop);
	  }
	  break;
	}
    }
#endif

#if ENABLE_COMPOSITE
  if (xev->type == wm->damage_event_base + XDamageNotify)
    {
      call_handlers_for_event (ctx->event_funcs.damage_notify,
			       xev,
			       xev->xany.window);
    }
  else
#endif
  switch (xev->type)
    {
    case ClientMessage:
      /* give the EWMH handler the first crack at it
       * FIXME: this should possibly be just an ordinary handler
       */
      if (!mb_wm_root_window_handle_message (wm->root_win,
					     (XClientMessageEvent *)xev))
	{
	  call_handlers_for_event (ctx->event_funcs.client_message,
				   &xev->xclient,
				   xev->xany.window);
	}
      break;
    case Expose:
      /* we do nothing */
      break;
    case MapRequest:
      call_handlers_for_event (ctx->event_funcs.map_request,
			       &xev->xmaprequest,
			       xev->xany.window);
      break;
    case MapNotify:
      call_handlers_for_event (ctx->event_funcs.map_notify,
			       &xev->xmap,
			       xev->xany.window);
      break;
    case UnmapNotify:
      call_handlers_for_event (ctx->event_funcs.unmap_notify,
			       &xev->xunmap,
			       xev->xunmap.window);
      break;
    case DestroyNotify:
      call_handlers_for_event (ctx->event_funcs.destroy_notify,
			       &xev->xdestroywindow,
			       xev->xany.window);
      break;
    case ConfigureNotify:
      call_handlers_for_event (ctx->event_funcs.configure_notify,
			       &xev->xconfigure,
			       xev->xconfigure.window);
      break;
    case ConfigureRequest:
      call_handlers_for_event (ctx->event_funcs.configure_request,
			       &xev->xconfigurerequest,
			       xev->xconfigurerequest.window);
      break;
    case KeyPress:
      call_handlers_for_event (ctx->event_funcs.key_press,
			       &xev->xkey,
			       xev->xproperty.window);
      break;
    case KeyRelease:
      call_handlers_for_event (ctx->event_funcs.key_release,
			       &xev->xkey,
			       xev->xproperty.window);
      break;
    case PropertyNotify:
      call_handlers_for_event (ctx->event_funcs.property_notify,
			       &xev->xproperty,
			       xev->xproperty.window);
      break;
    case ButtonPress:
      call_handlers_for_event (ctx->event_funcs.button_press,
			       &xev->xbutton,
			       xev->xany.window);
      break;
    case ButtonRelease:
      call_handlers_for_event (ctx->event_funcs.button_release,
			       &xev->xbutton,
			       xev->xany.window);
      break;
    case MotionNotify:
      call_handlers_for_event (ctx->event_funcs.motion_notify,
			       &xev->xmotion,
			       xev->xany.window);
      break;
    }

  mb_wm_main_context_remove_deleted_handlers (ctx);

  return False;
}

static Bool
mb_wm_main_context_spin_xevent (MBWMMainContext *ctx)
{
  MBWindowManager * wm = ctx->wm;
  XEvent xev;

  if (!XEventsQueued (wm->xdpy, QueuedAfterFlush))
    return False;

  XNextEvent(wm->xdpy, &xev);

  mb_wm_main_context_handle_x_event (&xev, ctx);

  return (XEventsQueued (wm->xdpy, QueuedAfterReading) != 0);
}

#if ! USE_GLIB_MAINLOOP
static Bool
mb_wm_main_context_spin_xevent_blocking (MBWMMainContext *ctx)
{
  MBWindowManager * wm = ctx->wm;
  XEvent xev;

  XNextEvent(wm->xdpy, &xev);

  mb_wm_main_context_handle_x_event (&xev, ctx);

  return (XEventsQueued (wm->xdpy, QueuedAfterReading) != 0);
}
#endif

void
mb_wm_main_context_loop (MBWMMainContext *ctx)
{
#if ! USE_GLIB_MAINLOOP
  MBWindowManager * wm = ctx->wm;

  while (True)
    {
      Bool sources;

      sources  = mb_wm_main_context_check_timeouts (ctx);
      sources |= mb_wm_main_context_check_fd_watches (ctx);

      if (!sources)
	{
	  /* No timeouts, idles, etc. -- wait for next
	   * X event
	   */
	  mb_wm_main_context_spin_xevent_blocking (ctx);
	}
      else
	{
	  /* Process any pending xevents */
	  while (mb_wm_main_context_spin_xevent (ctx));
	}

      if (wm->sync_type)
	mb_wm_sync (wm);
    }
#endif
}

Bool
mb_wm_main_context_spin_loop (MBWMMainContext *ctx)
{
#if USE_GLIB_MAINLOOP
  g_main_context_iteration (NULL, FALSE);
  return g_main_context_pending (NULL);
#else
  return mb_wm_main_context_spin_xevent (ctx);
#endif
}


unsigned long
mb_wm_main_context_x_event_handler_add (MBWMMainContext *ctx,
					Window           xwin,
					int              type,
					MBWMXEventFunc   func,
					void            *userdata)
{
  static unsigned long    ids = 0;
  MBWMXEventFuncInfo    * func_info;
#if ENABLE_COMPOSITE
  MBWindowManager       * wm = ctx->wm;
#endif

  ++ids;

  func_info           = mb_wm_util_malloc0(sizeof(MBWMXEventFuncInfo));
  func_info->func     = func;
  func_info->xwindow  = xwin;
  func_info->userdata = userdata;
  func_info->id       = ids;
  func_info->deleted  = False;

#if ENABLE_COMPOSITE
  if (type == wm->damage_event_base + XDamageNotify)
    {
      ctx->event_funcs.damage_notify =
	mb_wm_util_list_append (ctx->event_funcs.damage_notify, func_info);
    }
  else
#endif
  switch (type)
    {
    case Expose:
      free (func_info);
      break;
    case MapRequest:
      ctx->event_funcs.map_request =
	mb_wm_util_list_append (ctx->event_funcs.map_request, func_info);
      break;
    case MapNotify:
      ctx->event_funcs.map_notify=
	mb_wm_util_list_append (ctx->event_funcs.map_notify, func_info);
      break;
    case UnmapNotify:
      ctx->event_funcs.unmap_notify=
	mb_wm_util_list_append (ctx->event_funcs.unmap_notify, func_info);
      break;
    case DestroyNotify:
      ctx->event_funcs.destroy_notify =
	mb_wm_util_list_append (ctx->event_funcs.destroy_notify, func_info);
      break;
    case ConfigureNotify:
      ctx->event_funcs.configure_notify =
	mb_wm_util_list_append (ctx->event_funcs.configure_notify, func_info);
      break;
    case ConfigureRequest:
      ctx->event_funcs.configure_request =
	mb_wm_util_list_append (ctx->event_funcs.configure_request, func_info);
      break;
    case KeyPress:
      ctx->event_funcs.key_press =
	mb_wm_util_list_append (ctx->event_funcs.key_press, func_info);
      break;
    case KeyRelease:
      ctx->event_funcs.key_release =
	mb_wm_util_list_append (ctx->event_funcs.key_release, func_info);
      break;
    case PropertyNotify:
      ctx->event_funcs.property_notify =
	mb_wm_util_list_append (ctx->event_funcs.property_notify, func_info);
      break;
    case ButtonPress:
      ctx->event_funcs.button_press =
	mb_wm_util_list_append (ctx->event_funcs.button_press, func_info);
      break;
    case ButtonRelease:
      ctx->event_funcs.button_release =
	mb_wm_util_list_append (ctx->event_funcs.button_release, func_info);
      break;
    case MotionNotify:
      ctx->event_funcs.motion_notify =
	mb_wm_util_list_append (ctx->event_funcs.motion_notify, func_info);
      break;
    case ClientMessage:
      ctx->event_funcs.client_message =
	mb_wm_util_list_append (ctx->event_funcs.client_message, func_info);
      break;

    default:
      free (func_info);
      break;
    }

  return ids;
}

static void 
mb_wm_list_remove_deleted_handlers (MBWMList **l_start)
{
  MBWMList        * l = NULL;

  if (l_start)
    l = *l_start;

  while (l)
    {
      MBWMXEventFuncInfo * info = l->data;

      if (info->deleted)
	{
	  MBWMList * prev = l->prev;
	  MBWMList * next = l->next;

	  if (prev)
	    prev->next = next;
	  else
	    *l_start = next;

	  if (next)
	    next->prev = prev;

          memset (info, 0, sizeof(*info));
	  free (info);
	  free (l);

	  l = next;
	} else {
	  l = l->next;
	}
    }
}

static void
mb_wm_main_context_remove_deleted_handlers (MBWMMainContext *ctx)
{
  if (ctx->event_funcs.deleted_map_notify)
    {
      mb_wm_list_remove_deleted_handlers (&ctx->event_funcs.map_notify);
      ctx->event_funcs.deleted_map_notify = False;
    }

  if (ctx->event_funcs.deleted_unmap_notify)
    {
      mb_wm_list_remove_deleted_handlers (&ctx->event_funcs.unmap_notify);
      ctx->event_funcs.deleted_unmap_notify = False;
    }

  if (ctx->event_funcs.deleted_map_request)
    {
      mb_wm_list_remove_deleted_handlers (&ctx->event_funcs.map_request);
      ctx->event_funcs.deleted_map_request = False;
    }

  if (ctx->event_funcs.deleted_destroy_notify)
    {
      mb_wm_list_remove_deleted_handlers (&ctx->event_funcs.destroy_notify);
      ctx->event_funcs.deleted_destroy_notify = False;
    }

  if (ctx->event_funcs.deleted_configure_request)
    {
      mb_wm_list_remove_deleted_handlers (&ctx->event_funcs.configure_request);
      ctx->event_funcs.deleted_configure_request = False;
    }

  if (ctx->event_funcs.deleted_configure_notify)
    {
      mb_wm_list_remove_deleted_handlers (&ctx->event_funcs.configure_notify);
      ctx->event_funcs.deleted_configure_notify = False;
    }

  if (ctx->event_funcs.deleted_key_press)
    {
      mb_wm_list_remove_deleted_handlers (&ctx->event_funcs.key_press);
      ctx->event_funcs.deleted_key_press = False;
    }

  if (ctx->event_funcs.deleted_key_release)
    {
      mb_wm_list_remove_deleted_handlers (&ctx->event_funcs.key_release);
      ctx->event_funcs.deleted_key_release = False;
    }

  if (ctx->event_funcs.deleted_property_notify)
    {
      mb_wm_list_remove_deleted_handlers (&ctx->event_funcs.property_notify);
      ctx->event_funcs.deleted_property_notify = False;
    }

  if (ctx->event_funcs.deleted_button_press)
    {
      mb_wm_list_remove_deleted_handlers (&ctx->event_funcs.button_press);
      ctx->event_funcs.deleted_button_press = False;
    }

  if (ctx->event_funcs.deleted_button_release)
    {
      mb_wm_list_remove_deleted_handlers (&ctx->event_funcs.button_release);
      ctx->event_funcs.deleted_button_release = False;
    }

  if (ctx->event_funcs.deleted_motion_notify)
    {
      mb_wm_list_remove_deleted_handlers (&ctx->event_funcs.motion_notify);
      ctx->event_funcs.deleted_motion_notify = False;
    }

  if (ctx->event_funcs.deleted_client_message)
    {
      mb_wm_list_remove_deleted_handlers (&ctx->event_funcs.client_message);
      ctx->event_funcs.deleted_client_message = False;
    }

#if ENABLE_COMPOSITE
  if (ctx->event_funcs.deleted_damage_notify)
    {
      mb_wm_list_remove_deleted_handlers (&ctx->event_funcs.damage_notify);
      ctx->event_funcs.deleted_damage_notify = False;
    }
#endif
}

void
mb_wm_main_context_x_event_handler_remove (MBWMMainContext *ctx,
					   int              type,
					   unsigned long    id)
{
  MBWMList        * l = NULL;
  MBWMList        **l_start = NULL;

#if ENABLE_COMPOSITE
  MBWindowManager * wm = ctx->wm;

  if (type == wm->damage_event_base + XDamageNotify)
    {
      l_start = &ctx->event_funcs.damage_notify;
    }
  else
#endif
  switch (type)
    {
    case Expose:
      break;
    case MapRequest:
      ctx->event_funcs.deleted_map_request = True;
      l_start = &ctx->event_funcs.map_request;
      break;
    case MapNotify:
      ctx->event_funcs.deleted_map_notify = True;
      l_start = &ctx->event_funcs.map_notify;
      break;
    case UnmapNotify:
      ctx->event_funcs.deleted_unmap_notify = True;
      l_start = &ctx->event_funcs.unmap_notify;
      break;
    case DestroyNotify:
      ctx->event_funcs.deleted_destroy_notify = True;
      l_start = &ctx->event_funcs.destroy_notify;
      break;
    case ConfigureNotify:
      ctx->event_funcs.deleted_configure_notify = True;
      l_start = &ctx->event_funcs.configure_notify;
      break;
    case ConfigureRequest:
      ctx->event_funcs.deleted_configure_request = True;
      l_start = &ctx->event_funcs.configure_request;
      break;
    case KeyPress:
      ctx->event_funcs.deleted_key_press = True;
      l_start = &ctx->event_funcs.key_press;
      break;
    case KeyRelease:
      ctx->event_funcs.deleted_key_release = True;
      l_start = &ctx->event_funcs.key_release;
      break;
    case PropertyNotify:
      ctx->event_funcs.deleted_property_notify = True;
      l_start = &ctx->event_funcs.property_notify;
      break;
    case ButtonPress:
      ctx->event_funcs.deleted_button_press = True;
      l_start = &ctx->event_funcs.button_press;
      break;
    case ButtonRelease:
      ctx->event_funcs.deleted_button_release = True;
      l_start = &ctx->event_funcs.button_release;
      break;
    case MotionNotify:
      ctx->event_funcs.deleted_motion_notify = True;
      l_start = &ctx->event_funcs.motion_notify;
      break;
    case ClientMessage:
      ctx->event_funcs.deleted_client_message = True;
      l_start = &ctx->event_funcs.client_message;
      break;

    default:
      break;
    }

  if (l_start)
    l = *l_start;

  while (l)
    {
      MBWMXEventFuncInfo * info = l->data;

      if (info->id == id)
	{
	  info->deleted = True;
	  return;
	}

      l = l->next;
    }
}

#if ! USE_GLIB_MAINLOOP
static void
mb_wm_main_context_timeout_setup (MBWMTimeOutEventInfo * tinfo,
				  struct timeval       * current_time)
{
  int sec = tinfo->ms / 1000;
  int usec = (tinfo->ms - sec *1000) * 1000;

  sec  += current_time->tv_sec;
  usec += current_time->tv_usec;

  if (usec >= 1000000)
    {
      usec -= 1000000;
      sec++;
    }

  tinfo->triggers.tv_sec  = sec;
  tinfo->triggers.tv_usec = usec;
}

static Bool
mb_wm_main_context_handle_timeout (MBWMTimeOutEventInfo *tinfo,
				   struct timeval       *current_time)
{
  if (tinfo->triggers.tv_sec < current_time->tv_sec ||
      (tinfo->triggers.tv_sec == current_time->tv_sec &&
       tinfo->triggers.tv_usec <= current_time->tv_usec))
    {
      if (!tinfo->func (tinfo->userdata))
	return False;

      mb_wm_main_context_timeout_setup (tinfo, current_time);
    }

  return True;
}

/*
 * Returns false if no timeouts are present
 */
static Bool
mb_wm_main_context_check_timeouts (MBWMMainContext *ctx)
{
  MBWMList * l = ctx->event_funcs.timeout;
  struct timeval current_time;

  if (!l)
    return False;

  gettimeofday (&current_time, NULL);

  while (l)
    {
      MBWMTimeOutEventInfo * tinfo = l->data;

      if (!mb_wm_main_context_handle_timeout (tinfo, &current_time))
	{
	  MBWMList * prev = l->prev;
	  MBWMList * next = l->next;

	  if (prev)
	    prev->next = next;
	  else
	    ctx->event_funcs.timeout = next;

	  if (next)
	    next->prev = prev;

	  free (tinfo);
	  free (l);

	  l = next;
	}
      else
	l = l->next;
    }

  return True;
}
#endif /* !USE_GLIB_MAINLOOP */

unsigned long
mb_wm_main_context_timeout_handler_add (MBWMMainContext            *ctx,
					int                         ms,
					MBWindowManagerTimeOutFunc  func,
					void                       *userdata)
{
#if ! USE_GLIB_MAINLOOP
  static unsigned long ids = 0;
  MBWMTimeOutEventInfo * tinfo;
  struct timeval current_time;

  ++ids;

  tinfo = mb_wm_util_malloc0 (sizeof (MBWMTimeOutEventInfo));
  tinfo->func = func;
  tinfo->id = ids;
  tinfo->ms = ms;
  tinfo->userdata = userdata;

  gettimeofday (&current_time, NULL);
  mb_wm_main_context_timeout_setup (tinfo, &current_time);

  ctx->event_funcs.timeout =
    mb_wm_util_list_append (ctx->event_funcs.timeout, tinfo);

  return ids;

#else
  return g_timeout_add (ms, func, userdata);
#endif
}

void
mb_wm_main_context_timeout_handler_remove (MBWMMainContext *ctx,
					   unsigned long    id)
{
#if ! USE_GLIB_MAINLOOP
  MBWMList * l = ctx->event_funcs.timeout;

  while (l)
    {
        MBWMTimeOutEventInfo * info = l->data;

      if (info->id == id)
	{
	  MBWMList * prev = l->prev;
	  MBWMList * next = l->next;

	  if (prev)
	    prev->next = next;
	  else
	    ctx->event_funcs.timeout = next;

	  if (next)
	    next->prev = prev;

	  free (info);
	  free (l);

	  return;
	}

      l = l->next;
    }
#else
  g_source_remove (id);
#endif
}

unsigned long
mb_wm_main_context_fd_watch_add (MBWMMainContext           *ctx,
				 MBWMIOChannel             *channel,
				 MBWMIOCondition            events,
				 MBWindowManagerFdWatchFunc func,
				 void                      *userdata)
{
#if ! USE_GLIB_MAINLOOP
  static unsigned long ids = 0;
  MBWMFdWatchInfo * finfo;
  struct pollfd * fds;

  ++ids;

  finfo = mb_wm_util_malloc0 (sizeof (MBWMFdWatchInfo));
  finfo->func = func;
  finfo->id = ids;
  finfo->channel = channel;
  finfo->events = events;
  finfo->userdata = userdata;

  ctx->event_funcs.fd_watch =
    mb_wm_util_list_append (ctx->event_funcs.fd_watch, finfo);

  ctx->n_poll_fds++;
  ctx->poll_fds = realloc (ctx->poll_fds, sizeof (struct pollfd));

  fds = ctx->poll_fds + (ctx->n_poll_fds - 1);
  fds->fd = *channel;
  fds->events = events;

  return ids;

#else
  return g_io_add_watch (channel, events, func, userdata);
#endif
}

void
mb_wm_main_context_fd_watch_remove (MBWMMainContext *ctx,
				    unsigned long    id)
{
#if ! USE_GLIB_MAINLOOP
  MBWMList * l = ctx->event_funcs.fd_watch;

  while (l)
    {
        MBWMFdWatchInfo * info = l->data;

      if (info->id == id)
	{
	  MBWMList * prev = l->prev;
	  MBWMList * next = l->next;

	  if (prev)
	    prev->next = next;
	  else
	    ctx->event_funcs.fd_watch = next;

	  if (next)
	    next->prev = prev;

	  free (info);
	  free (l);

	  return;
	}

      l = l->next;
    }

  ctx->n_poll_fds--;
  ctx->poll_cache_dirty = True;
#else
  g_source_remove (id);
#endif
}

MBWMIOChannel *
mb_wm_main_context_io_channel_new (int fd)
{
#if ! USE_GLIB_MAINLOOP
  MBWMIOChannel * c = mb_wm_util_malloc0 (sizeof (MBWMIOChannel));
  *c = fd;
  return c;
#else
  return g_io_channel_unix_new (fd);
#endif
}


void
mb_wm_main_context_io_channel_destroy (MBWMIOChannel * channel)
{
#if ! USE_GLIB_MAINLOOP
  if (channel)
    free (channel);
#else
  g_io_channel_unref (channel);
#endif
}

int
mb_wm_main_context_io_channel_get_fd (MBWMIOChannel * channel)
{
#if ! USE_GLIB_MAINLOOP
  return *channel;
#else
  return g_io_channel_unix_get_fd (channel);
#endif
}

#if ! USE_GLIB_MAINLOOP
static void
mb_wm_main_context_setup_poll_cache (MBWMMainContext *ctx)
{
  MBWMList *l = ctx->event_funcs.fd_watch;
  int i = 0;

  if (!ctx->poll_cache_dirty)
    return;

  ctx->poll_fds = realloc (ctx->poll_fds, ctx->n_poll_fds);

  while (l)
    {
      MBWMFdWatchInfo *info = l->data;

      ctx->poll_fds[i].fd     = *(info->channel);
      ctx->poll_fds[i].events = info->events;

      l = l->next;
      ++i;
    }

  ctx->poll_cache_dirty = False;
}

static Bool
mb_wm_main_context_check_fd_watches (MBWMMainContext * ctx)
{
  int ret;
  int i = 0;
  MBWMList * l = ctx->event_funcs.fd_watch;
  Bool removal = False;

  if (!ctx->n_poll_fds)
    return False;

  mb_wm_main_context_setup_poll_cache (ctx);

  ret = poll (ctx->poll_fds, ctx->n_poll_fds, 0);

  if (ret < 0)
    {
      MBWM_DBG ("Poll failed.");
      return True;
    }

  if (ret == 0)
    return True;

  while (l)
    {
      MBWMFdWatchInfo *info = l->data;

      if (ctx->poll_fds[i].revents & ctx->poll_fds[i].events)
	{
	  Bool zap = !info->func (info->channel, ctx->poll_fds[i].revents,
				  info->userdata);

	  if (zap)
	    {
	      MBWMList * prev = l->prev;
	      MBWMList * next = l->next;

	      if (prev)
		prev->next = next;
	      else
		ctx->event_funcs.fd_watch = next;

	      if (next)
		next->prev = prev;

	      free (info);
	      free (l);

	      ctx->n_poll_fds--;

	      removal = True;

	      l = next;
	    }
	  else
	    l = l->next;
	}
      else
	l = l->next;

      ++i;
    }

  ctx->poll_cache_dirty = removal;

  return True;
}
#endif
