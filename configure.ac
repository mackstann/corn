AC_PREREQ([2.54])
AC_INIT([corn], [0.0.0], [nick@incise.org])
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

AC_CHECK_HEADERS(errno.h locale.h signal.h stdarg.h stdlib.h string.h sys/stat.h sys/types.h)

PKG_CHECK_MODULES(XINE, [libxine >= 1.0.0])
AC_SUBST(XINE_CFLAGS)
AC_SUBST(XINE_LIBS)

PKG_CHECK_MODULES(GLIB, [glib-2.0])
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

PKG_CHECK_MODULES(GNOMEVFS, [gnome-vfs-2.0 >= 2.10])
AC_SUBST(GNOMEVFS_CFLAGS)
AC_SUBST(GNOMEVFS_LIBS)

PKG_CHECK_MODULES(GTHREAD, [gthread-2.0])
AC_SUBST(GTHREAD_CFLAGS)
AC_SUBST(GTHREAD_LIBS)

PKG_CHECK_MODULES(DBUS_GLIB, [dbus-glib-1])
AC_SUBST(DBUS_GLIB_CFLAGS)
AC_SUBST(DBUS_GLIB_LIBS)

PKG_CHECK_MODULES(UNIQUE, [unique-1.0])
AC_SUBST(UNIQUE_CFLAGS)
AC_SUBST(UNIQUE_LIBS)

PKG_CHECK_MODULES(GIO, [gio-2.0])
AC_SUBST(GIO_CFLAGS)
AC_SUBST(GIO_LIBS)

PKG_CHECK_MODULES(SQLITE, [sqlite3])
AC_SUBST(SQLITE_CFLAGS)
AC_SUBST(SQLITE_LIBS)

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
  Makefile
  po/Makefile.in
  corn/Makefile
])

AM_MAINTAINER_MODE

AC_ARG_ENABLE([debug],
  [  --enable-debug          enable debugging],
  [], [enable_debug="no"])

CFLAGS="-std=gnu99 -Wall"

if test "$enable_debug" = "yes"; then
  CFLAGS="$CFLAGS -ggdb -fno-inline"
else
  CFLAGS="$CFLAGS -Os"
fi

AC_OUTPUT

AC_MSG_RESULT
AC_MSG_RESULT([configure complete, now type \"make\"])
