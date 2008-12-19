
if THEME_PNG
PNG_SRC = mb-wm-theme-png.c mb-wm-theme-png.h
endif

COMMON_SRC = mb-wm-theme.h mb-wm-theme.c mb-wm-theme-xml.h mb-wm-theme-xml.c

pkgincludedir = $(includedir)/@MBWM2_INCDIR@/theme-engines

if ENABLE_LIBMATCHBOX
pkginclude_HEADERS = mb-wm-theme.h  mb-wm-theme-png.h  mb-wm-theme-xml.h
noinst_LTLIBRARIES = libmb-theme.la
libmb_theme_la_SOURCES = $(COMMON_SRC) $(PNG_SRC)
libmb_theme_la_CFLAGS =
else
noinst_LIBRARIES = libmb-theme.a
libmb_theme_a_SOURCES = $(COMMON_SRC) $(PNG_SRC)
endif

INCLUDES = @MBWM_INCS@ @MBWM_CFLAGS@ -DDATADIR=\"$(datadir)\"

MAINTAINERCLEANFILES = config.h.in Makefile.in
