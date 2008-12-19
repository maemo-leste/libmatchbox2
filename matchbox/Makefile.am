if ENABLE_COMPOSITE
comp=comp-mgr
complib= \
	client-types/libmb-wm-client-override.la \
	$(comp)/libmatchbox-window-manager-2-compmgr.la
endif

MAINTAINERCLEANFILES = Makefile.in

pkgincludedir = $(includedir)/@MBWM2_INCDIR@

if ENABLE_LIBMATCHBOX
SUBDIRS = core client-types theme-engines $(comp)

lib_LTLIBRARIES = libmatchbox2-@MBWM2_API_VERSION@.la


libmatchbox2_@MBWM2_API_VERSION@_la_LIBADD = \
			core/libmatchbox-window-manager-2-core.la \
                        client-types/libmb-wm-client-app.la \
			client-types/libmb-wm-client-desktop.la \
			client-types/libmb-wm-client-dialog.la \
			client-types/libmb-wm-client-input.la \
			client-types/libmb-wm-client-menu.la \
			client-types/libmb-wm-client-note.la \
			client-types/libmb-wm-client-panel.la \
			theme-engines/libmb-theme.la \
			$(complib)

libmatchbox2_@MBWM2_API_VERSION@_la_SOURCES =

pkginclude_HEADERS = *.h

else
SUBDIRS = core client-types theme-engines $(comp) managers
endif

