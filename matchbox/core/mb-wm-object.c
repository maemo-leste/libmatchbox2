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

#include "mb-wm.h"

#if MBWM_WANT_DEBUG
#include <execinfo.h>
#endif

static MBWMObjectClassInfo **ObjectClassesInfo  = NULL;
static MBWMObjectClass     **ObjectClasses  = NULL;
static int                   ObjectClassesAllocated = 0;
static int                   NObjectClasses = 0;

#if MBWM_WANT_DEBUG
#define MBWM_OBJECT_TRACE_DEPTH 3
/*
 * Increased for each ref call and decreased for each unref call
 */
MBWMList *alloc_objects = NULL;

void
mb_wm_object_dump ()
{
  MBWMList * l = alloc_objects;

  if (!l)
    {
      fprintf (stderr, "=== There currently are no allocated objects === \n");
      return;
    }

  fprintf (stderr, "=== Currently allocated objects === \n");

  while (l)
    {
      int i;
      MBWMObject * o = l->data;
      const MBWMObjectClass * k = MB_WM_OBJECT_GET_CLASS (o);

      fprintf (stderr, "Object of type %s, allocated from:\n",
	       k->klass_name);


      for (i = 1; i < MBWM_OBJECT_TRACE_DEPTH; ++i)
	{
	  char * s = o->trace_strings[i];
	  while (s && *s && *s != '(')
	    s++;

	  fprintf (stderr, "    %s\n", s);
	}

      l = l->next;
    }

  fprintf (stderr, "=== Currently allocated objects end === \n");

}

#endif

#define N_CLASSES_PREALLOC 10
#define N_CLASSES_REALLOC_STEP  5

void
mb_wm_object_init(void)
{
  if (!ObjectClasses || !ObjectClassesInfo)
    {
      ObjectClasses = mb_wm_util_malloc0 (sizeof(void*) * N_CLASSES_PREALLOC);
      ObjectClassesInfo = mb_wm_util_malloc0 (
                                sizeof(void*) * N_CLASSES_PREALLOC);

      if (ObjectClasses && ObjectClassesInfo)
        ObjectClassesAllocated = N_CLASSES_PREALLOC;
    }
}

static void
mb_wm_object_class_init_recurse (MBWMObjectClass *klass,
				 MBWMObjectClass *parent)
{
  if (parent->parent)
    mb_wm_object_class_init_recurse (klass, parent->parent);

  if (parent->class_init)
    parent->class_init (klass);
}

static void
mb_wm_object_class_init (MBWMObjectClass *klass)
{
  if (klass->parent)
    mb_wm_object_class_init_recurse (klass, klass->parent);

  if (klass->class_init)
    klass->class_init (klass);
}

int
mb_wm_object_register_class (MBWMObjectClassInfo *info,
			     int                  parent_type,
			     int                  flags)
{
  MBWMObjectClass *klass;

  if (NObjectClasses >= ObjectClassesAllocated)
    {
      int byte_len;
      int new_offset;
      int new_byte_len;

      new_offset = ObjectClassesAllocated;
      ObjectClassesAllocated += N_CLASSES_REALLOC_STEP;

      byte_len     = sizeof(void *) * (ObjectClassesAllocated);
      new_byte_len = sizeof(void *) * (ObjectClassesAllocated - new_offset);

      ObjectClasses     = realloc (ObjectClasses,     byte_len);
      ObjectClassesInfo = realloc (ObjectClassesInfo, byte_len);

      if (!ObjectClasses || !ObjectClassesInfo)
	return 0;

      memset (ObjectClasses + new_offset    , 0, new_byte_len);
      memset (ObjectClassesInfo + new_offset, 0, new_byte_len);
    }

  ObjectClassesInfo[NObjectClasses] = info;

  klass             = mb_wm_util_malloc0(info->klass_size);
  klass->init       = info->instance_init;
  klass->destroy    = info->instance_destroy;
  klass->class_init = info->class_init;
  klass->type       = NObjectClasses + 1;

  if (parent_type != 0)
    klass->parent = ObjectClasses[parent_type-1];

  ObjectClasses[NObjectClasses] = klass;

  mb_wm_object_class_init (klass);

  return 1 + NObjectClasses++;
}

void *
mb_wm_object_ref (MBWMObject *this)
{
  if (!this)
    {
      MBWM_DBG("### Warning: called with NULL ###");
      return this;
    }

  this->refcnt++;

  MBWM_TRACE_MSG (OBJ_REF, "### REF ###");

  return this;
}

static void
mb_wm_object_destroy_recursive (const MBWMObjectClass * klass,
				MBWMObject *this)
{
  /* Destruction needs to happen top to bottom */
  MBWMObjectClass *parent_klass = klass->parent;

  if (klass->destroy)
    klass->destroy (this);

  if (parent_klass)
    mb_wm_object_destroy_recursive (parent_klass, this);
}

int
mb_wm_object_get_refcount (MBWMObject *this)
{
  return this->refcnt;
}

void
mb_wm_object_unref (MBWMObject *this)
{
  if (!this)
    {
      MBWM_DBG("### Warning: called with NULL ###");
      return;
    }


  this->refcnt--;

  if (this->refcnt == 0)
    {
      MBWM_NOTE (OBJ_UNREF, "=== DESTROYING OBJECT type %lu ===",
		 this->klass->type);

      mb_wm_object_destroy_recursive (MB_WM_OBJECT_GET_CLASS (this),
				      this);

      free (this);

#if MBWM_WANT_DEBUG
      alloc_objects = mb_wm_util_list_remove (alloc_objects, this);
#endif
    }
}

static int
mb_wm_object_init_recurse (MBWMObject *obj, MBWMObjectClass *parent,
			   va_list vap)
{
  va_list vap2;

  va_copy (vap2, vap);

  if (parent->parent)
    if (!mb_wm_object_init_recurse (obj, parent->parent, vap2))
      {
        va_end (vap2);
        return 0;
      }

  if (parent->init)
    if (!parent->init (obj, vap))
      {
        va_end (vap2);
        return 0;
      }

  va_end (vap2);

  return 1;
}

static int
mb_wm_object_init_object (MBWMObject *obj, va_list vap)
{
  va_list vap2;

  va_copy(vap2, vap);

  if (obj->klass->parent)
    if (!mb_wm_object_init_recurse (obj, obj->klass->parent, vap2))
      {
        va_end(vap2);
        return 0;
      }

  if (obj->klass->init)
    if (!obj->klass->init(obj, vap))
      {
        va_end(vap2);
        return 0;
      }

  va_end(vap2);

  return 1;
}


MBWMObject*
mb_wm_object_new (int type, ...)
{
  MBWMObjectClassInfo *info;
  MBWMObject          *obj;
  va_list              vap;

  va_start(vap, type);

  info = ObjectClassesInfo[type-1];

  obj = mb_wm_util_malloc0 (info->instance_size);

  obj->klass = MB_WM_OBJECT_CLASS(ObjectClasses[type-1]);

  if (!mb_wm_object_init_object (obj, vap))
    {
      free (obj);
      va_end(vap);
      return NULL;
    }


  mb_wm_object_ref (obj);

  va_end(vap);

#if MBWM_WANT_DEBUG
 {
   void * trace[MBWM_OBJECT_TRACE_DEPTH];

   alloc_objects = mb_wm_util_list_append (alloc_objects, obj);
   obj->trace_depth   = backtrace (trace, sizeof(trace)/sizeof(void*));
   obj->trace_strings = backtrace_symbols (trace, obj->trace_depth);
 }
#endif

  return obj;
}

unsigned long
mb_wm_object_signal_connect (MBWMObject             *obj,
			     unsigned long           signal,
			     MBWMObjectCallbackFunc  func,
			     void                   *userdata)
{
  static unsigned long id_counter = 0;
  MBWMFuncInfo *func_info;

  MBWM_ASSERT(func != NULL);

  func_info           = mb_wm_util_malloc0(sizeof(MBWMFuncInfo));
  func_info->func     = (void*)func;
  func_info->userdata = userdata;
  func_info->data     = mb_wm_object_ref (obj);
  func_info->signal   = signal;
  func_info->id       = id_counter++;

  obj->callbacks =
    mb_wm_util_list_append (obj->callbacks, func_info);

  return func_info->id;
}

void
mb_wm_object_signal_disconnect (MBWMObject    *obj,
				unsigned long  id)
{
  MBWMList  *item = obj->callbacks;

  while (item)
    {
      MBWMFuncInfo* info = item->data;

      if (info->id == id)
	{
	  MBWMList * prev = item->prev;
	  MBWMList * next = item->next;

	  if (prev)
	    prev->next = next;
	  else
	    obj->callbacks = next;

	  if (next)
	    next->prev = prev;

	  mb_wm_object_unref (MB_WM_OBJECT (info->data));

	  free (info);
	  free (item);

	  return;
	}

      item = item->next;
    }

  MBWM_DBG ("### Warning: did not find signal handler %lu ###", id);
}

void
mb_wm_object_signal_emit (MBWMObject    *obj,
			  unsigned long  signal)
{
    MBWMList  *item = obj->callbacks;

    while (item)
      {
	MBWMFuncInfo* info = item->data;

	if (info->signal & signal)
	  {
	    if (((MBWMObjectCallbackFunc)info->func) (obj,
						      signal,
						      info->userdata))
	      {
		break;
	      }
	  }

	item = item->next;
      }
}

gboolean
mb_wm_object_is_descendant (MBWMObject *obj, int type)
{
  const MBWMObjectClass *considering = MB_WM_OBJECT_GET_CLASS (obj);

  while (1)
    {
      if (considering->type == type) {
	return TRUE;
      }

      if (considering->parent) {
	considering = considering->parent;
      } else {
	/* ran out of options */
	return FALSE;
      }
  }
}

#if 0

/* ----- Test code -------- */

typedef struct Foo
{
  MBWMObject parent;

  int        hello;
}
Foo;

typedef struct FooClass
{
  MBWMObjectClass parent;

}
FooClass;

void
mb_wm_foo_init (MBWMObject *obj)
{
  printf("%s() called\n", __func__);
}

void
mb_wm_foo_destroy (MBWMObject *obj)
{
  printf("%s() called\n", __func__);
}

int
mb_wm_foo_get_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (FooClass),
	sizeof (Foo),  		/* Instance */
	mb_wm_foo_init,
	mb_wm_foo_destroy,
	NULL
      };

      type = mb_wm_object_register_class (&info);

      printf("type: %i\n", type);
    }

  return type;
}

Foo*
mb_wm_foo_new (int val)
{
  Foo *foo;

  foo = (Foo*)mb_wm_object_new (mb_wm_foo_get_class_type ());

  /* call init */

  foo->hello = val;

  return foo;
}


int
main (int argc, char **argv)
{
  Foo *foo, *foo2;

  mb_wm_object_init();

  printf("%s() called init, about to call new\n", __func__);

  foo = mb_wm_foo_new (10);
  foo2 = mb_wm_foo_new (10);

  printf("%s() foo->hello is %i\n", __func__, foo->hello);

  mb_wm_object_unref (MB_WM_OBJECT(foo));
}

#endif
