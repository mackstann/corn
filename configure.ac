AC_PREREQ([2.54])
AC_INIT([corn], [3.21], [http://bugs.com/])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([corn/main.c])

AC_PREFIX_DEFAULT([/usr/local])
test "$prefix" = "NONE" && prefix=$ac_default_prefix

AC_PROG_CC
AC_C_CONST
AC_C_INLINE
AC_PROG_INSTALL

AM_GNU_GETTEXT_VERSION(0.11.5)
AM_GNU_GETTEXT([external])

#AC_CHECK_HEADERS(ctype.h fcntl.h locale.h signal.h string.h stdio.h stdlib.h)
#AC_CHECK_HEADERS(unistd.h sys/stat.h sys/select.h sys/time.h sys/wait.h)

PKG_CHECK_MODULES(XINE, [libxine >= 1.0.0])
AC_SUBST(XINE_CFLAGS)
AC_SUBST(XINE_LIBS)

PKG_CHECK_MODULES(GDK_PIXBUF, [gdk-pixbuf-2.0])
AC_SUBST(GDK_PIXBUF_CFLAGS)
AC_SUBST(GDK_PIXBUF_LIBS)

PKG_CHECK_MODULES(GLIB, [glib-2.0])
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

PKG_CHECK_MODULES(GNOMEVFS, [gnome-vfs-2.0])
AC_SUBST(GNOMEVFS_CFLAGS)
AC_SUBST(GNOMEVFS_LIBS)

PKG_CHECK_MODULES(GTHREAD, [gthread-2.0])
AC_SUBST(GTHREAD_CFLAGS)
AC_SUBST(GTHREAD_LIBS)

PKG_CHECK_MODULES(DBUS_GLIB, [dbus-glib-1])
AC_SUBST(DBUS_GLIB_CFLAGS)
AC_SUBST(DBUS_GLIB_LIBS)

AC_ARG_ENABLE(gconf,
  [  --disable-gconf         disable use of gconf (for embedded targets)],
  [], [enable_gconf="yes"])

if test "$enable_gconf" = "yes"; then
  PKG_CHECK_MODULES(GCONF, [gconf-2.0])

  AC_PATH_PROG(GCONFTOOL, gconftool-2, no)
  if test x"$GCONFTOOL" = xno; then
    AC_MSG_ERROR([gconftool-2 executable not found in your path - should be installed with GConf])
  fi
  AM_GCONF_SOURCE_2

  AC_SUBST(GCONF_CFLAGS)
  AC_SUBST(GCONF_LIBS)
  AC_DEFINE(USE_GCONF, [1], [Use GConf for configuration])
else
  AM_CONDITIONAL(GCONF_SCHEMAS_INSTALL, false)

  AC_MSG_WARN([Not building with GConf])
fi
AM_CONDITIONAL(USE_GCONF, test "$enable_gconf" = "yes")

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
  Makefile
  m4/Makefile 
  po/Makefile.in
  corn/Makefile
  pixmaps/Makefile
])

AM_MAINTAINER_MODE

AC_ARG_ENABLE([debug],
  [  --enable-debug          enable debugging],
  [], [enable_debug="no"])

CFLAGS="-std=gnu99 -Wall -pedantic -Wsign-compare -Waggregate-return -Wcast-qual -Wbad-function-cast -Wpointer-arith"

if test "$enable_debug" = "yes"; then
  CFLAGS="$CFLAGS -g -fno-inline"
else
  CFLAGS="$CFLAGS -Os"
fi

AC_OUTPUT

AC_MSG_RESULT
AC_MSG_RESULT([configure complete, now type \"make\"])
