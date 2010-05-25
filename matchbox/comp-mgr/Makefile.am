
core_h    = mb-wm-comp-mgr.h
core_c    = mb-wm-comp-mgr.c

if COMP_MGR_BACKEND
if ENABLE_CLUTTER_COMPOSITE_MANAGER
clutter_h = mb-wm-comp-mgr-clutter.h
clutter_c = mb-wm-comp-mgr-clutter.c
endif
endif

pkgincludedir = $(includedir)/@MBWM2_INCDIR@/comp-mgr

if ENABLE_LIBMATCHBOX
pkginclude_HEADERS = mb-wm-comp-mgr-clutter.h  mb-wm-comp-mgr.h
noinst_LTLIBRARIES = libmatchbox-window-manager-2-compmgr.la
libmatchbox_window_manager_2_compmgr_la_SOURCES = $(core_h) $(core_c) \
						  $(clutter_h) $(clutter_c)
libmatchbox_window_manager_2_compmgr_la_CFLAGS = @MBWM_INCS@ @MBWM_CFLAGS@
else
noinst_LIBRARIES = libmatchbox-window-manager-2-compmgr.a

libmatchbox_window_manager_2_compmgr_a_SOURCES = $(core_h) $(core_c)	   \
						 $(clutter_h) $(clutter_c)
libmatchbox_window_manager_2_compmgr_a_CFLAGS = @MBWM_INCS@ @MBWM_CFLAGS@
endif

MAINTAINERCLEANFILES = Makefile.in

