if ENABLE_COMPOSITE
compmgr_libs = libmb-wm-client-override.a
compmgr_ltlibs = libmb-wm-client-override.la
endif

THEME_LIBS = @MBWM_THEME_LIBS@/libmb-theme.a

INCLUDES     = @MBWM_INCS@ @MBWM_CFLAGS@
LDADD        =$(THEME_LIBS)
DEPENDENCIES = $(THEME_LIBS)

pkgincludedir = $(includedir)/@MBWM2_INCDIR@/client-types

if ENABLE_LIBMATCHBOX
pkginclude_HEADERS = mb-wm-client-app.h \
		     mb-wm-client-dialog.h \
		     mb-wm-client-menu.h \
		     mb-wm-client-override.h \
		     mb-wm-client-desktop.h \
		     mb-wm-client-input.h \
		     mb-wm-client-note.h \
		     mb-wm-client-panel.h
noinst_LTLIBRARIES = libmb-wm-client-app.la	\
	             libmb-wm-client-panel.la	\
	             libmb-wm-client-dialog.la	\
	             libmb-wm-client-note.la	\
		     libmb-wm-client-input.la	\
		     libmb-wm-client-desktop.la    \
		     libmb-wm-client-menu.la	\
		     $(compmgr_ltlibs)

libmb_wm_client_app_la_SOURCES     = mb-wm-client-app.c mb-wm-client-app.h
libmb_wm_client_panel_la_SOURCES   = mb-wm-client-panel.c mb-wm-client-panel.h
libmb_wm_client_dialog_la_SOURCES  = mb-wm-client-dialog.c mb-wm-client-dialog.h
libmb_wm_client_note_la_SOURCES    = mb-wm-client-note.c mb-wm-client-note.h
libmb_wm_client_input_la_SOURCES   = mb-wm-client-input.c mb-wm-client-input.h
libmb_wm_client_desktop_la_SOURCES = mb-wm-client-desktop.c mb-wm-client-desktop.h
libmb_wm_client_menu_la_SOURCES    = mb-wm-client-menu.c mb-wm-client-menu.h
libmb_wm_client_override_la_SOURCES= mb-wm-client-override.c \
				    mb-wm-client-override.h
libmb_wm_client_app_la_CFLAGS =
libmb_wm_client_panel_la_CFLAGS =
libmb_wm_client_dialog_la_CFLAGS =
libmb_wm_client_note_la_CFLAGS =
libmb_wm_client_input_la_CFLAGS =
libmb_wm_client_desktop_la_CFLAGS =
libmb_wm_client_menu_la_CFLAGS =
libmb_wm_client_override_la_CFLAGS =
else
noinst_LIBRARIES = libmb-wm-client-app.a	\
	           libmb-wm-client-panel.a	\
	           libmb-wm-client-dialog.a	\
	           libmb-wm-client-note.a	\
		   libmb-wm-client-input.a	\
		   libmb-wm-client-desktop.a    \
		   libmb-wm-client-menu.a	\
		   $(compmgr_libs)

libmb_wm_client_app_a_SOURCES     = mb-wm-client-app.c mb-wm-client-app.h
libmb_wm_client_panel_a_SOURCES   = mb-wm-client-panel.c mb-wm-client-panel.h
libmb_wm_client_dialog_a_SOURCES  = mb-wm-client-dialog.c mb-wm-client-dialog.h
libmb_wm_client_note_a_SOURCES    = mb-wm-client-note.c mb-wm-client-note.h
libmb_wm_client_input_a_SOURCES   = mb-wm-client-input.c mb-wm-client-input.h
libmb_wm_client_desktop_a_SOURCES = mb-wm-client-desktop.c mb-wm-client-desktop.h
libmb_wm_client_menu_a_SOURCES    = mb-wm-client-menu.c mb-wm-client-menu.h
libmb_wm_client_override_a_SOURCES= mb-wm-client-override.c \
				    mb-wm-client-override.h
endif


MAINTAINERCLEANFILES = config.h.in Makefile.in
