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

#ifndef _HAVE_MB_OBJECT_H
#define _HAVE_MB_OBJECT_H

#include <stdarg.h>
#include <matchbox/core/mb-wm-object-props.h>

typedef struct MBWMObject       MBWMObject;
typedef struct MBWMObjectClass  MBWMObjectClass;

typedef void (*MBWMObjFunc)     (MBWMObject* obj);
typedef int  (*MBWMObjVargFunc) (MBWMObject* obj, va_list vap);
typedef void (*MBWMClassFunc)   (MBWMObjectClass* klass);

#define MB_WM_TYPE_OBJECT 0
#define MB_WM_OBJECT(x) ((MBWMObject*)(x))
#define MB_WM_OBJECT_CLASS(x) ((MBWMObjectClass*)(x))
#define MB_WM_OBJECT_TYPE(x) (((MBWMObject*)(x))->klass->type)
#define MB_WM_OBJECT_GET_CLASS(x) (mb_wm_object_get_class (MB_WM_OBJECT(x)))
#define MB_WM_OBJECT_GET_PARENT_CLASS(x) \
    ((mb_wm_object_get_class (MB_WM_OBJECT(x)))->parent)

typedef enum  MBWMObjectClassType
{
  MB_WM_OBJECT_TYPE_CLASS     = 0,
  MB_WM_OBJECT_TYPE_ABSTRACT,
  MB_WM_OBJECT_TYPE_SINGLETON
}
MBWMObjectClassType;

/**
 * Description of a class, used when registering an object.
 */
typedef struct MBWMObjectClassInfo
{
  size_t              klass_size;
  size_t              instance_size;
  MBWMObjVargFunc     instance_init;
  MBWMObjFunc         instance_destroy;
  MBWMClassFunc       class_init;
}
MBWMObjectClassInfo;

/**
 * Class for MBWMObject.
 */
struct MBWMObjectClass
{
  int              type;
  MBWMObjectClass *parent;
  MBWMObjVargFunc  init;
  MBWMObjFunc      destroy;
  MBWMClassFunc    class_init;

#if MBWM_WANT_DEBUG
  const char         *klass_name;
#endif
};

/**
 * The root of the object hierarchy.
 */
struct MBWMObject
{
  MBWMObjectClass *klass;
  int              refcnt;

  MBWMList        *callbacks;

#if MBWM_WANT_DEBUG
  char           **trace_strings;
  int              trace_depth;
#endif
};

/* returns True to stop signal emission */
typedef Bool (*MBWMObjectCallbackFunc) (MBWMObject *obj,
					int         mask,
					void       *userdata);


void
mb_wm_object_init(void);

int
mb_wm_object_register_class (MBWMObjectClassInfo *info,
			     int                  parent_type,
			     int                  flags);

void *
mb_wm_object_ref (MBWMObject *this);

void
mb_wm_object_unref (MBWMObject *this);

MBWMObject*
mb_wm_object_new (int type, ...);

const MBWMObjectClass*
mb_wm_object_get_class (MBWMObject *this);

unsigned long
mb_wm_object_signal_connect (MBWMObject            *obj,
			     unsigned long          signal,
			     MBWMObjectCallbackFunc func,
			     void                  *userdata);

void
mb_wm_object_signal_disconnect (MBWMObject    *obj,
				unsigned long  id);

void
mb_wm_object_signal_emit (MBWMObject *obj, unsigned long signal);

#if MBWM_WANT_DEBUG
void
mb_wm_object_dump ();
#endif

#endif
