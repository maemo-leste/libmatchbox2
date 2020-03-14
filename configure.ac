# mbwm2 package version number, (as distinct from shared library version)
# An odd minor number indicates in-progress development, (eg. from svn)
# An even minor number indicates a released version.
m4_define([mbwm2_major_version], [0])
m4_define([mbwm2_minor_version], [1])
m4_define([mbwm2_micro_version], [0])

m4_define([mbwm2_version],
          [mbwm2_major_version.mbwm2_minor_version.mbwm2_micro_version])

m4_define([mbwm2_api_version],
          [mbwm2_major_version.mbwm2_minor_version])

AC_PREREQ([2.60])
AC_INIT([matchbox-window-manager-2], mbwm2_version, [mallum@handhelds.org])
AC_CONFIG_SRCDIR([matchbox/core/mb-window-manager.c])

AM_INIT_AUTOMAKE()

MBWM2_API_VERSION=mbwm2_api_version
AC_SUBST(MBWM2_API_VERSION)

MBWM2_INCDIR=matchbox-wm-2/$MBWM2_API_VERSION/matchbox
AC_SUBST(MBWM2_INCDIR)

MBWM2_PKGINCDIR=matchbox-wm-2/$MBWM2_API_VERSION/
AC_SUBST(MBWM2_PKGINCDIR)

clutter_package=clutter-0.8

AM_MAINTAINER_MODE

AC_PROG_CC
AC_USE_SYSTEM_EXTENSIONS
AM_PROG_LIBTOOL

AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h string.h])
AC_C_CONST
AC_CHECK_FUNCS([memset strdup strncasecmp])

needed_pkgs="x11 "

AC_ARG_ENABLE(debug,
  [  --enable-debug          Enable verbose debugging output],
  [want_debug=$enableval], [want_debug=no])

AC_ARG_ENABLE(png-theme,
  [  --enable-png-theme      Enable PNG-based theme],
  [png_theme=$enableval], [png_theme=yes])

AC_ARG_WITH(pango,
  [  --with-pango            Use of pango for text layout],
  [use_pango=$withval], [use_pango=no])

dnl Temporarily set to yes so it builds properly.  See #89473.
AC_ARG_WITH(gtk,
  [  --with-gtk              With GTK integration support],
  [use_gtk=$withval], [use_gtk=yes])

if test "x$use_pango" = "xyes"; then
  needed_pkgs="$needed_pkgs pangoxft "
fi

if ! test "x$use_pango" = "xyes" -o "x$use_pango" = "xyes"; then
  needed_pkgs="$needed_pkgs xft "
fi

dnl Temporarily set to yes so it builds properly.  See #89473.
if test "x$use_gtk" != "xno"; then
  needed_pkgs="$needed_pkgs gtk+-2.0 "
fi

if test "x$png_theme" = "xyes"; then
  needed_pkgs="$needed_pkgs libpng "
fi

needed_pkgs="$needed_pkgs $clutter_package xcomposite xdamage "
COMPOSITE_MANAGER_DEFINE="-DCOMPOSITE_MANAGER_CLUTTER"
AC_SUBST(COMPOSITE_MANAGER_DEFINE)

AC_SUBST(MBWM_DEB_VERSION, $DEB_VERSION)

MBWM_WANT_DEBUG=0
if test "x$want_debug" = "xyes"; then
  MBWM_WANT_DEBUG=1
  MBWM_DEBUG_CFLAGS="-O0 $MBWM_DEBUG_CFLAGS -g"
  MBWM_DEBUG_LDFLAGS="$MBWM_DEBUG_LDFLAGS -rdynamic -g"
fi
AC_SUBST(MBWM_WANT_DEBUG)

needed_pkgs="$needed_pkgs glib-2.0 "
USE_GLIB_MAINLOOP=1
AC_SUBST(USE_GLIB_MAINLOOP)

USE_PANGO=0
if test "$use_pango" = yes; then
  USE_PANGO=1
fi
AC_SUBST(USE_PANGO)

THEME_PNG=0
AM_CONDITIONAL(THEME_PNG, [test "x$png_theme" = "xyes"])
if test "$png_theme" = yes; then
  THEME_PNG=1
fi
AC_SUBST(THEME_PNG)

# only build simple theme if png theme disabled
THEME_SIMPLE=0
AM_CONDITIONAL(THEME_SIMPLE, [test "x$use_png" != "xyes"])
if test "$use_png" != yes; then
  THEME_SIMPLE=1
fi
AC_SUBST(THEME_SIMPLE)

dnl Temporarily set to yes so it builds properly.  See #89473.
USE_GTK=0
AM_CONDITIONAL(USE_GTK, test "x$use_gtk" = "xyes")
if test x"$use_gtk" != xno; then
  USE_GTK=1
fi
AC_SUBST(USE_GTK)

PKG_CHECK_MODULES(MBWM, $needed_pkgs)
MBWM2_PKGREQUIRES="$MBWM2_PKGREQUIRES $needed_pkgs"
AC_SUBST(MBWM2_PKGREQUIRES)

ENABLE_COMPOSITE=1
AC_SUBST(ENABLE_COMPOSITE)
AM_CONDITIONAL(ENABLE_COMPOSITE, [true])

ENABLE_CLUTTER_COMPOSITE_MANAGER=1
AC_SUBST(ENABLE_CLUTTER_COMPOSITE_MANAGER)

AM_CONDITIONAL(ENABLE_CLUTTER_COMPOSITE_MANAGER, [true])
clutter_backend=$($PKG_CONFIG $clutter_package --variable=backend)
if test x"$clutter_backend" = xglx; then
  AC_DEFINE(HAVE_CLUTTER_GLX, [1], [Using clutter glx backend])
elif test x"$clutter_backend" = xeglx; then
  AC_DEFINE(HAVE_CLUTTER_EGLX, [1], [Using clutter eglx backend])
fi

COMP_MGR_BACKEND=1
AC_SUBST(COMP_MGR_BACKEND)
AM_CONDITIONAL(COMP_MGR_BACKEND, [true])

AM_CONDITIONAL(ENABLE_LIBMATCHBOX, [true])

PKG_CHECK_MODULES(XFIXES, xfixes >= 4.0, have_xfixes=yes, have_xfixes=no)

if test x$have_xfixes = xyes; then
  AC_DEFINE(HAVE_XFIXES, [1], [Use XFixes ext to really hide cursor])
  MBWM2_PKGREQUIRES="$MBWM2_PKGREQUIRES xfixes >= 0.4"
fi

PKG_CHECK_MODULES(XEXT, xext, have_xext=yes, have_xext=no)

if test x$have_xext = xyes; then
  AC_DEFINE(HAVE_XEXT, [1], [Use Xext ext])
  MBWM2_PKGREQUIRES="$MBWM2_PKGREQUIRES xext"
fi

PKG_CHECK_MODULES(XCURSOR, xcursor, have_xcursor=yes, have_xcursor=no)

if test x$have_xcursor = xyes; then
  AC_DEFINE(HAVE_XCURSOR, [1], [Use XCursor to sync pointer themes])
fi

AC_C_BIGENDIAN
if test "x$ac_cv_c_bigendian" = "xyes"; then
	CPPFLAGS="$CPPFLAGS -DBYTE_ORDER=BIG_ENDIAN"
else
	CPPFLAGS="$CPPFLAGS -DBYTE_ORDER=LITTLE_ENDIAN"
fi

MBWM_INCS='-I$(top_srcdir) -I$(top_srcdir)/matchbox/core -I$(top_srcdir)/matchbox/client-types -I$(top_srcdir)/matchbox/theme-engines -I$(top_srcdir)/matchbox/comp-mgr -I$(top_builddir)'
MBWM_CORE_LIB='$(top_builddir)/matchbox/core/libmatchbox-window-manager-2-core.a'
MBWM_CLIENT_BUILDDIR='$(top_builddir)/matchbox/client-types'
MBWM_THEME_BUILDDIR='$(top_builddir)/matchbox/theme-engines'
MBWM_COMPMGR_BUILDDIR='$(top_builddir)/matchbox/comp-mgr'
# -Wall should be turned on later
MBWM_CFLAGS="$MBWM_CFLAGS $MBWM_DEBUG_CFLAGS $XFIXES_CFLAGS $XEXT_CFLAGS $XCURSOR_CFLAGS -g -Werror -DG_DISABLE_CAST_CHECKS"
MBWM_LIBS="$MBWM_LIBS $XFIXES_LIBS $XEXT_LIBS $XCURSOR_LIBS $MBWM_EXTRA_LIBS"

AC_SUBST([MBWM_CFLAGS])
AC_SUBST([MBWM_LIBS])
AC_SUBST([MBWM_CORE_LIB])
AC_SUBST([MBWM_INCS])
AC_SUBST([MBWM_CLIENT_BUILDDIR])
AC_SUBST([MBWM_THEME_BUILDDIR])
AC_SUBST([MBWM_COMPMGR_BUILDDIR])
AC_SUBST([MBWM_DEBUG_CFLAGS])
AC_SUBST([MBWM_DEBUG_LDFLAGS])

AC_OUTPUT([
Makefile
matchbox/Makefile
matchbox/mb-wm-config.h
matchbox/core/Makefile
matchbox/client-types/Makefile
matchbox/comp-mgr/Makefile
matchbox/theme-engines/Makefile
data/Makefile
data/themes/Makefile
data/themes/Default/Makefile
data/themes/PngSample/Makefile
data/libmatchbox2.pc
])

echo "
           Matchbox Window Manager II
           ==========================

    Directories:
        source code location:   ${srcdir}
        install prefix        :   ${prefix}

    Extension:
	Xfixes                :   ${have_xfixes}
	Xext                  :   ${have_xext}
	Xcursor               :   ${have_xcursor}

    Themes:
	PNG theme             :   ${png_theme}
	Pango integration     :   ${use_pango}

    Miscel:
	Debugging output      :   ${want_debug}
"
