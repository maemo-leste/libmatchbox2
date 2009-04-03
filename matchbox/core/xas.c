/* Asynchronous Xlib hack utiltys lib
 *
 * Copyright (c) 2005 Matthew Allum
 *
 * Contains portions of code from Metacity and Xlib thus;
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

#include "xas.h"

#if MBWM_WANT_DEBUG
#include "mb-wm-debug.h"
#include <stdio.h>
#define XAS_DBG(x, a...) \
if (mbwm_debug_flags & MBWM_DEBUG_XAS) \
 fprintf(stderr, __FILE__ ":%d,%s() " x "\n", __LINE__, __func__, ##a)
#else
#define XAS_DBG(x, a...) do {} while (0)
#endif
#define XAS_MARK() XAS_DBG("--mark--");

#ifdef MBWM_WANT_ASSERT
#include <assert.h>
#define XAS_ASSERT(x) assert(x)
#else
#define XAS_ASSERT(x) do {} while (0)
#endif

#define XAS_ALIGN_VALUE(this, boundary) \
  (( ((unsigned long)(this)) + (((unsigned long)(boundary)) -1)) & (~(((unsigned long)(boundary))-1)))

#define NEED_REPLIES
#include <X11/Xlibint.h>

#ifndef NULL
#define NULL ((void*)0)
#endif

typedef struct XasTask            XasTask;
typedef struct XasTaskGetProperty XasTaskGetProperty;
typedef struct XasTaskGetWinAttr  XasTaskGetWinAttr;
typedef struct XasTaskGetGeom     XasTaskGetGeom;

#define XAS_TASK(t) (XasTask*)(t)

typedef enum XasTaskType
{
  XAS_TASK_UNKNOWN,
  XAS_TASK_GET_PROPERTY,
  XAS_TASK_GET_WIN_ATTR,
  XAS_TASK_GET_GEOM,

} XasTaskType;

struct XasContext
{
  _XAsyncHandler async;
  Display       *xdpy;
  XasTask       *tasks_pending;
  int            n_tasks_pending;
  XasTask       *tasks_completed;
  int            n_tasks_completed;
};

struct XasTask
{
  XasTask       *next;
  XasTaskType    type;
  XasContext    *ctx;
  unsigned long  request_seq;
  Bool           have_reply;
  int            error;

};

struct XasTaskGetProperty
{
  XasTask        task;

  Window         window;
  Atom           property;

  Atom           actual_type;
  int            actual_format;

  unsigned long  n_items;
  unsigned long  bytes_after;
  unsigned char *data;
};

struct XasTaskGetWinAttr
{
  XasTask            task;
  Window             window;
  XasWindowAttributes *attr;
};

struct XasTaskGetGeom
{
  XasTask            task;
  Drawable           drw;
  Window             root;
  int                x;
  int                y;
  unsigned int       width;
  unsigned int       height;
  unsigned int       border;
  unsigned int       depth;
};

static XasTask*
task_list_append(XasTask *tasks, XasTask *task)
{
  XasTask *t = NULL;

  /* FIXME: use hash on seq_req key instead ? */

  if (tasks == NULL) return task;

  for (t = tasks; t->next != NULL; t = t->next);

  t->next = task;

  return tasks;
}

XasTask*
task_list_remove(XasTask *tasks, XasTask *task)
{
  XasTask *t = NULL, *p = NULL;

  if (task == tasks)
    {
      t = task->next;
      tasks->next = NULL;
      return t;
    }

  p = tasks;
  t = tasks->next;

  while (t != NULL)
    {
      if (t == task)
	{
	  p->next = t->next;
	  task->next = NULL;
	  return tasks;
	}
      p = t; t = t->next;
    }

  return tasks;
}

static void
task_init(XasContext *ctx, XasTask *task, XasTaskType type)
{
  task->ctx         = ctx;
  task->next        = NULL;
  task->type        = type;
  task->request_seq = ctx->xdpy->request;
  task->have_reply  = False;
}

static void
task_complete(XasContext *ctx, XasTask *task)
{
  ctx->tasks_pending = task_list_remove(ctx->tasks_pending, task);
  ctx->n_tasks_pending--;
  ctx->tasks_completed = task_list_append(ctx->tasks_completed, task);
  ctx->n_tasks_completed++;
}

static XasTask*
xas_find_task_for_request_seq(XasContext    *ctx,
			      XasTask       *task_list,
			      unsigned long  request_seq)
{
  XasTask *task = NULL;

  task = task_list;

  while (task != NULL)
    {
      if (task->request_seq == request_seq)
	return task;
      task = task->next;
    }

  XAS_DBG("Failed to find task\n");
  return NULL;
}

static Bool
xas_async_get_geom_handler (XasContext         *ctx,
			    XasTaskGetGeom  *task,
			    xReply             *rep,
			    char               *buf,
			    int                 len)
{
  Display                   *dpy;
  xGetGeometryReply         *repl;
  xGetGeometryReply          replbuf;

  dpy = ctx->xdpy;

  task->task.have_reply = True;

  task_complete (ctx, XAS_TASK(task));

  if (rep->generic.type == X_Error)
    {
      xError errbuf;
      task->task.error = rep->error.errorCode;
      _XGetAsyncReply (dpy, (char *)&errbuf, rep, buf, len,
                       (SIZEOF (xError) - SIZEOF (xReply)) >> 2,
                       False);
      return True;
    }

  repl = (xGetGeometryReply *)
    _XGetAsyncReply(dpy, (char *)&replbuf, rep, buf, len,
		    (SIZEOF(xGetGeometryReply) - SIZEOF(xReply)) >> 2,
		    True);

  task->root   = repl->root;
  task->x      = cvtINT16toInt (repl->x);
  task->y      = cvtINT16toInt (repl->y);
  task->width  = repl->width;
  task->height = repl->height;
  task->border = repl->borderWidth;
  task->depth  = repl->depth;

  return True;
}


static Bool
xas_async_get_win_attr_handler (XasContext         *ctx,
				XasTaskGetWinAttr  *task,
				xReply             *rep,
				char               *buf,
				int                 len)
{
  Display                   *dpy;
  xGetWindowAttributesReply  replbuf;
  xGetWindowAttributesReply *repl;

  dpy = ctx->xdpy;

  task->task.have_reply = True;

  task_complete (ctx, XAS_TASK(task));

  if (rep->generic.type == X_Error)
    {
      xError errbuf;
      task->task.error = rep->error.errorCode;
      _XGetAsyncReply (dpy, (char *)&errbuf, rep, buf, len,
                       (SIZEOF (xError) - SIZEOF (xReply)) >> 2,
                       False);
      return True;
    }

  repl = (xGetWindowAttributesReply *)
    _XGetAsyncReply(dpy, (char *)&replbuf, rep, buf, len,
		    (SIZEOF(xGetWindowAttributesReply) - SIZEOF(xReply)) >> 2,
		    True);

  task->attr = (XasWindowAttributes *)Xmalloc(sizeof(XasWindowAttributes));

  if (task->attr == NULL)
    {
      task->task.error = BadAlloc;
      return True;
    }

  task->attr->class = repl->class;
  task->attr->bit_gravity = repl->bitGravity;
  task->attr->win_gravity = repl->winGravity;
  task->attr->backing_store = repl->backingStore;
  task->attr->backing_planes = repl->backingBitPlanes;
  task->attr->backing_pixel = repl->backingPixel;
  task->attr->save_under = repl->saveUnder;
  task->attr->colormap = repl->colormap;
  task->attr->map_installed = repl->mapInstalled;
  task->attr->map_state = repl->mapState;
  task->attr->all_event_masks = repl->allEventMasks;
  task->attr->your_event_mask = repl->yourEventMask;
  task->attr->do_not_propagate_mask = repl->doNotPropagateMask;
  task->attr->override_redirect = repl->override;
  task->attr->visual = _XVIDtoVisual (dpy, repl->visualID);

  return True;
}

static Bool
xas_async_get_property_handler (XasContext         *ctx,
				XasTaskGetProperty *task,
				xReply             *rep,
				char               *buf,
				int                 len)
{
  Display            *dpy;
  xGetPropertyReply  replbuf;
  xGetPropertyReply *reply;
  int                bytes_read;

  /* Code and comments here wripped out of Metacity, Copyright
   * Havoc Pennington.
   */

  /* FIXME: Really check this code is really reliable/safe.*/

  dpy = ctx->xdpy;

  XAS_DBG ("seeing request seq %ld buflen %d",
	   dpy->last_request_read, len);

  task_complete (ctx, XAS_TASK(task));

  /* read bytes so far */
  bytes_read = SIZEOF (xReply);

  if (rep->generic.type == X_Error)
    {
      xError errbuf;

      task->task.error = rep->error.errorCode;

      /* We return True (meaning we consumed the reply)
       * because otherwise it would invoke the X error handler,
       * and an async API is useless if you have to synchronously
       * trap X errors. Also GetProperty can always fail, pretty
       * much, so trapping errors is always what you want.
       *
       * We have to eat all the error reply data here.
       * (kind of a charade as we know sizeof(xError) == sizeof(xReply))
       *
       * Passing discard = True seems to break things; I don't understand
       * why, because there should be no extra data in an error reply,
       * right?
       */
      _XGetAsyncReply (dpy, (char *)&errbuf, rep, buf, len,
                       (SIZEOF (xError) - bytes_read) >> 2, /* 32-bit words */
                       False); /* really seems like it should be True */
      return True;
    }

  XAS_DBG ("already read %d bytes reading %d more for total of %d; generic.length = %ld",
	   bytes_read, (SIZEOF (xGetPropertyReply) - bytes_read) >> 2,
	   SIZEOF (xGetPropertyReply), rep->generic.length);

  reply = (xGetPropertyReply *)
    _XGetAsyncReply (dpy, (char *)&replbuf, rep, buf, len,
                     (SIZEOF (xGetPropertyReply) - bytes_read) >> 2,
                     False); /* False means expecting more data to follow,
                              * don't eat the rest of the reply
                              */

  bytes_read = SIZEOF (xGetPropertyReply);

  XAS_DBG ("have reply propertyType = %ld format = %d n_items = %ld",
	   reply->propertyType, reply->format, reply->nItems);

  /* This is all copied from XGetWindowProperty().  Not sure we should
   * LockDisplay(). Not sure I'm passing the right args to
   * XGetAsyncData(). Not sure about a lot of things.
   */

  /* LockDisplay (dpy); */

  if (reply->propertyType != None)
    {
      long nbytes, netbytes;

      /* this alignment macro from orbit2 */

      switch (reply->format)
        {
          /*
           * One extra byte is malloced than is needed to contain the property
           * data, but this last byte is null terminated and convenient for
           * returning string properties, so the client doesn't then have to
           * recopy the string to make it null terminated.
           */
        case 8:
          nbytes = reply->nItems;
          /* there's padding to word boundary */
          netbytes = XAS_ALIGN_VALUE (nbytes, 4);
          if (nbytes + 1 > 0 &&
              (task->data = (unsigned char *) Xmalloc ((unsigned)nbytes + 1)))
            {

              XAS_DBG ("already read %d bytes using %ld, more eating %ld more",
		       bytes_read, nbytes, netbytes);

              /* _XReadPad (dpy, (char *) task->data, netbytes); */
              _XGetAsyncData (dpy, (char *)task->data, buf, len,
                              bytes_read, nbytes,
                              netbytes);
            }
          break;

        case 16:
          nbytes = reply->nItems * sizeof (short);
          netbytes = reply->nItems << 1;
          netbytes = XAS_ALIGN_VALUE (netbytes, 4); /* to word boundary */
          if (nbytes + 1 > 0 &&
              (task->data = (unsigned char *) Xmalloc ((unsigned)nbytes + 1)))
            {
              XAS_DBG ("%s: already read %d bytes using %ld more, eating %ld more\n",
                      __FUNCTION__, bytes_read, nbytes, netbytes);

              /* _XRead16Pad (dpy, (short *) task->data, netbytes); */
              _XGetAsyncData (dpy, (char*)task->data, buf, len,
                              bytes_read, nbytes, netbytes);
            }
          break;

        case 32:
          /* NOTE buffer is in longs to match XGetWindowProperty() */
          nbytes = reply->nItems * sizeof (long);
          netbytes = reply->nItems << 2; /* wire size always 32 bits though */
          if (nbytes + 1 > 0 &&
              (task->data = (unsigned char *) Xmalloc ((unsigned)nbytes + 1)))
            {
              XAS_DBG ("already read %d bytes using %ld more, eating %ld more",
		       bytes_read, nbytes, netbytes);

              /* We have to copy the XGetWindowProperty() crackrock
               * and get format 32 as long even on 64-bit platforms.
               */
              if (sizeof (long) == 8)
                {
                  unsigned char *netdata;
                  unsigned char *lptr;
                  unsigned char *end_lptr;

                  /* Store the 32-bit values in the end of the array */
                  netdata = task->data + nbytes / 2;

                  _XGetAsyncData (dpy, (char*)netdata, buf, len,
                                  bytes_read, netbytes,
                                  netbytes);

                  /* Now move the 32-bit values to the front */

                  lptr = task->data;
                  end_lptr = task->data + nbytes;
                  while (lptr != end_lptr)
                    {
                      *(long*) lptr = *(CARD32*) netdata;
                      lptr += sizeof (long);
                      netdata += sizeof (CARD32);
                    }
                }
              else
                {
                  /* Here the wire format matches our actual format */
                  _XGetAsyncData (dpy, (char*)task->data, buf, len,
                                  bytes_read, netbytes,
                                  netbytes);
                }
            }
          break;

        default:
          /*
           * This part of the code should never be reached.  If it is,
           * the server sent back a property with an invalid format.
           * This is a BadImplementation error.
           *
           * However this async GetProperty API doesn't report errors
           * via the standard X mechanism, so don't do anything about
           * it, other than store it in task->error.
           */
	  task->task.error = BadImplementation;
          nbytes = netbytes = 0L;
          break;
        }

      if (task->data == NULL)
        {
          task->task.error = BadAlloc;

          XAS_DBG ("already read %d bytes eating %ld", bytes_read, netbytes);

          /* _XEatData (dpy, (unsigned long) netbytes); */
          _XGetAsyncData (dpy, NULL, buf, len, bytes_read, 0, netbytes);

          /* UnlockDisplay (dpy); */
          return BadAlloc; /* not Success */
        }

      (task->data)[nbytes] = '\0';
    }

  XAS_DBG ("have data");

  task->actual_type = reply->propertyType;
  task->actual_format = reply->format;
  task->n_items = reply->nItems;
  task->bytes_after = reply->bytesAfter;

  /* UnlockDisplay (dpy); */

  return True;
}

static Bool
xas_async_handler (Display *dpy,
		   xReply  *rep,
		   char    *buf,
		   int      len,
		   XPointer data)
{
  XasContext *ctx = (XasContext *)data;
  XasTask    *task;

  XAS_ASSERT(ctx->xdpy == dpy);

  if (!ctx->tasks_pending)
    return False;

  task = xas_find_task_for_request_seq(ctx,
				       ctx->tasks_pending,
				       dpy->last_request_read);
  if (!task)
    return False;

  switch (task->type)
    {
    case XAS_TASK_GET_PROPERTY:
      return xas_async_get_property_handler (ctx,
					     (XasTaskGetProperty*)task,
					     rep, buf, len);
    case XAS_TASK_GET_WIN_ATTR:
      return xas_async_get_win_attr_handler (ctx,
					     (XasTaskGetWinAttr*)task,
					     rep, buf, len);
    case XAS_TASK_GET_GEOM:
      return xas_async_get_geom_handler (ctx,
					 (XasTaskGetGeom*)task,
					 rep, buf, len);
    case XAS_TASK_UNKNOWN:
    default:
      /* Should never get here */
      return False;
    }

  return False;
}

/* public */

XasContext*
xas_context_new(Display *xdpy)
{
  XasContext *ctx = NULL;

  ctx = (XasContext *)Xmalloc(sizeof(XasContext));

  ctx->xdpy          = xdpy;
  ctx->async.next    = xdpy->async_handlers;
  ctx->async.handler = xas_async_handler;
  ctx->async.data    = (XPointer) ctx;
  ctx->xdpy->async_handlers = &ctx->async;

  ctx->tasks_pending     = NULL;
  ctx->n_tasks_pending   = 0;
  ctx->tasks_completed   = NULL;
  ctx->n_tasks_completed = 0;

  return ctx;
}

void
xas_context_destroy(XasContext *ctx)
{
  DeqAsyncHandler (ctx->xdpy, &ctx->async);

  /* FIXME: empty pending and completed lists */

  free(ctx);
}

XasCookie
xas_get_property(XasContext *ctx,
		 Window      win,
		 Atom        property,
		 long        offset,
		 long        length,
		 Bool        delete,
		 Atom        req_type)
{
  Display            *dpy ;
  XasTaskGetProperty *task;
  xGetPropertyReq    *req;

  LockDisplay (ctx->xdpy);

  dpy = ctx->xdpy; 		/* GetReq() needs this */

  task = Xcalloc (1, sizeof (XasTaskGetProperty));
  if (task == NULL)
    {
      UnlockDisplay (dpy);
      return 0;
    }

  GetReq (GetProperty, req);
  req->window     = win;
  req->property   = property;
  req->type       = req_type;
  req->delete     = delete;
  req->longOffset = offset;
  req->longLength = length;

  /* needed ? error.sequenceNumber = ctx->xdpy->request;*/

  task_init(ctx, XAS_TASK(task), XAS_TASK_GET_PROPERTY);

  task->window   = win;
  task->property = property;

  ctx->tasks_pending = task_list_append(ctx->tasks_pending, &task->task);

  ctx->n_tasks_pending ++;

  UnlockDisplay (dpy);

  SyncHandle ();

  return task->task.request_seq;
}

Bool
xas_have_reply(XasContext          *ctx,
	       XasCookie            cookie)
{
  return (xas_find_task_for_request_seq(ctx, ctx->tasks_completed, cookie) != NULL);
}


Status
xas_get_property_reply(XasContext          *ctx,
		       XasCookie            cookie,
		       Atom                *actual_type_return,
		       int                 *actual_format_return,
		       unsigned long       *nitems_return,
		       unsigned long       *bytes_after_return,
		       unsigned char      **prop_return,
		       int                 *x_error_code)
{
  Display            *dpy;
  XasTaskGetProperty *task   = NULL;
  Bool                result = False;

  dpy = ctx->xdpy; /* For SyncHandle (); */

  if (x_error_code) *x_error_code = 0; /* No error as yet */

  task = (XasTaskGetProperty *) xas_find_task_for_request_seq(ctx,
							      ctx->tasks_completed,
							      cookie);
  if (task == NULL)
    {
      XAS_DBG("Failed to find cookie");
      return False;
    }

  if (!task->task.error)
    {
      *actual_type_return   = task->actual_type;
      *actual_format_return = task->actual_format;
      *nitems_return        = task->n_items;
      *bytes_after_return   = task->bytes_after;
      *prop_return          = task->data;

      result         = True;
    }
  else
    {
      XAS_DBG("is error");
      if (x_error_code)
	*x_error_code = task->task.error;
      result = False;;
    }

  SyncHandle ();

  ctx->tasks_completed = task_list_remove(ctx->tasks_completed,
					  XAS_TASK(task));
  ctx->n_tasks_completed--;

  XFree (task);

  return result;
}

XasCookie
xas_get_window_attributes(XasContext        *ctx,
			  Window             win)
{
  xResourceReq       *req;
  Display            *dpy ;
  XasTaskGetWinAttr  *task;

  LockDisplay (ctx->xdpy);

  dpy = ctx->xdpy; 		/* GetReq() needs this */

  task = Xcalloc (1, sizeof (XasTaskGetWinAttr));
  if (task == NULL)
    {
      UnlockDisplay (dpy);
      return 0;
    }

  GetResReq(GetWindowAttributes, win, req);

  task_init(ctx, XAS_TASK(task), XAS_TASK_GET_WIN_ATTR);

  task->window = win;

  ctx->tasks_pending = task_list_append(ctx->tasks_pending, &task->task);
  ctx->n_tasks_pending ++;

  UnlockDisplay (dpy);

  SyncHandle ();

  return task->task.request_seq;
}

XasWindowAttributes*
xas_get_window_attributes_reply(XasContext          *ctx,
				XasCookie            cookie,
				int                 *x_error_code)
{
  Display            *dpy;
  XasTaskGetWinAttr  *task   = NULL;
  XasWindowAttributes  *result = NULL;

  dpy = ctx->xdpy; /* For SyncHandle (); */

  if (x_error_code) *x_error_code = 0; /* No error as yet */

  task = (XasTaskGetWinAttr *) xas_find_task_for_request_seq(ctx,
							      ctx->tasks_completed,
							      cookie);

  if (task == NULL)
      return NULL;

  if (task->task.type != XAS_TASK_GET_WIN_ATTR)
    return NULL;

  if (!task->task.error)
    {
      result = task->attr;
    }
  else
    {
      XAS_DBG("is error");
      if (x_error_code)
	*x_error_code = task->task.error;
    }

  SyncHandle ();

  ctx->tasks_completed = task_list_remove(ctx->tasks_completed,
					  XAS_TASK(task));
  ctx->n_tasks_completed--;

  XFree (task);

  return result;
}

XasCookie
xas_get_geometry(XasContext        *ctx,
		 Drawable           d)
{
  xResourceReq       *req;
  Display            *dpy ;
  XasTaskGetGeom     *task;

  LockDisplay (ctx->xdpy);

  dpy = ctx->xdpy; 		/* GetReq() needs this */

  task = Xcalloc (1, sizeof (XasTaskGetGeom));
  if (task == NULL)
    {
      UnlockDisplay (dpy);
      return 0;
    }

  GetResReq(GetGeometry, d, req);

  task_init(ctx, XAS_TASK(task), XAS_TASK_GET_GEOM);

  task->drw = d;

  ctx->tasks_pending = task_list_append(ctx->tasks_pending, &task->task);
  ctx->n_tasks_pending ++;

  UnlockDisplay (dpy);

  SyncHandle ();

  return task->task.request_seq;
}

Status
xas_get_geometry_reply (XasContext   *ctx,
			XasCookie     cookie,
			int          *x_return,
			int          *y_return,
			unsigned int *width_return,
			unsigned int *height_return,
			unsigned int *border_width_return,
			unsigned int *depth_return,
			int          *x_error_code)
{
  Display            *dpy;
  XasTaskGetGeom  *task   = NULL;
  Status              result = False;

  dpy = ctx->xdpy; /* For SyncHandle (); */

  if (x_error_code) *x_error_code = 0; /* No error as yet */

  task = (XasTaskGetGeom *) xas_find_task_for_request_seq(ctx,
							     ctx->tasks_completed,
							     cookie);

  if (task == NULL)
    {
      XAS_DBG("Failed to find task\n");
      return False;
    }

  if (task->task.type != XAS_TASK_GET_GEOM)
    {
      XAS_DBG("Found task, but type different to expected ( %i vs %i )\n",
	      task->task.type, XAS_TASK_GET_GEOM);
      return False;
    }

  if (!task->task.error)
    {
      *x_return            = task->x;
      *y_return            = task->y;
      *width_return        = task->width;
      *height_return       = task->height;
      *border_width_return = task->border;
      *depth_return        = task->depth;
      result = True;
    }
  else
    {
      XAS_DBG("is error");
      if (x_error_code)
	*x_error_code = task->task.error;
    }

  SyncHandle ();

  ctx->tasks_completed = task_list_remove(ctx->tasks_completed,
					  XAS_TASK(task));
  ctx->n_tasks_completed--;

  XFree (task);

  return result;
}



