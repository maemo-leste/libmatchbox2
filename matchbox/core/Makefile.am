
core_h  = mb-wm.h             	\
          mb-wm-macros.h      	\
          mb-wm-debug.h       	\
          mb-wm-object.h      	\
          mb-wm-object-props.h 	\
          mb-wm-client.h      	\
          mb-wm-client-base.h 	\
	  mb-wm-client-window.h \
	  mb-wm-root-window.h 	\
          mb-wm-stack.h       	\
          mb-wm-util.h        	\
          mb-wm-types.h       	\
	  mb-wm-atoms.h       	\
          mb-wm-layout.h      	\
          mb-wm-props.h       	\
          mb-wm-keys.h        	\
          mb-wm-decor.h        	\
	  mb-window-manager.h	\
	  mb-wm-main-context.h	\
          xas.h

core_c  = mb-wm-object.c      	\
          mb-wm-debug.c       	\
          mb-wm-client.c      	\
          mb-wm-client-base.c 	\
	  mb-wm-client-window.c \
	  mb-wm-root-window.c   \
          mb-wm-stack.c       	\
          mb-wm-props.c       	\
          mb-wm-util.c        	\
	  mb-wm-atoms.c       	\
          mb-wm-layout.c      	\
          mb-wm-keys.c        	\
          mb-wm-decor.c       	\
	  mb-window-manager.c	\
	  mb-wm-main-context.c	\
          xas.c

pkgincludedir = $(includedir)/@MBWM2_INCDIR@/core

if ENABLE_LIBMATCHBOX
pkginclude_HEADERS = $(core_h)
noinst_LTLIBRARIES =  libmatchbox-window-manager-2-core.la
libmatchbox_window_manager_2_core_la_SOURCES = $(core_h) $(core_c)
libmatchbox_window_manager_2_core_la_CFLAGS =  @MBWM_INCS@ @MBWM_CFLAGS@
else
noinst_LIBRARIES = libmatchbox-window-manager-2-core.a
libmatchbox_window_manager_2_core_a_SOURCES = $(core_h) $(core_c)
libmatchbox_window_manager_2_core_a_CFLAGS =  @MBWM_INCS@ @MBWM_CFLAGS@
endif

MAINTAINERCLEANFILES = Makefile.in
