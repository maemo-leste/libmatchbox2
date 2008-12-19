INCLUDES = 					       \
	@MBWM_INCS@				       \
	@MBWM_CFLAGS@

CLIENT_LIBS =						\
        @MBWM_CLIENT_BUILDDIR@/libmb-wm-client-panel.a	\
        @MBWM_CLIENT_BUILDDIR@/libmb-wm-client-dialog.a	\
        @MBWM_CLIENT_BUILDDIR@/libmb-wm-client-note.a	\
        @MBWM_CLIENT_BUILDDIR@/libmb-wm-client-app.a	\
        @MBWM_CLIENT_BUILDDIR@/libmb-wm-client-input.a	\
        @MBWM_CLIENT_BUILDDIR@/libmb-wm-client-desktop.a	\
        @MBWM_CLIENT_BUILDDIR@/libmb-wm-client-menu.a

if ENABLE_COMPOSITE
COMPMGR_LIBS=@MBWM_COMPMGR_BUILDDIR@/libmatchbox-window-manager-2-compmgr.a \
             @MBWM_CLIENT_BUILDDIR@/libmb-wm-client-override.a
endif

THEME_LIBS = @MBWM_THEME_BUILDDIR@/libmb-theme.a

bin_PROGRAMS = matchbox-window-manager-2-simple

matchbox_window_manager_2_simple_SOURCES =		\
	matchbox-window-manager-2-simple.c

matchbox_window_manager_2_simple_LDFLAGS =		\
	$(MBWM_DEBUG_LDFLAGS)				\
	$(LDFLAGS)

matchbox_window_manager_2_simple_LDADD =		\
	@MBWM_LIBS@					\
        @MBWM_CORE_LIB@					\
	$(THEME_LIBS)					\
	$(CLIENT_LIBS)					\
	$(COMPMGR_LIBS)

matchbox_window_manager_2_simple_DEPENDENCIES =		\
        @MBWM_CORE_LIB@					\
	$(THEME_LIBS)					\
        $(CLIENT_LIBS)					\
	$(COMPMGR_LIBS)

MAINTAINERCLEANFILES = config.h.in Makefile.in
