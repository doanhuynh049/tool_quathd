#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([viewport_scaler], [1.0.0], [http://www.igel.co.jp/])
AC_CONFIG_SRCDIR([viewport_scaler.c])
AC_CONFIG_HEADERS([config.h])

# Initialize Automake
AM_INIT_AUTOMAKE([foreign dist-bzip2])

# Initialize libtool
AC_PROG_LIBTOOL

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h unistd.h])
AC_DEFINE([_GNU_SOURCE], 1, [Use GNU extensions])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_INT32_T
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([gettimeofday])

# check dependencies
PKG_CHECK_MODULES([WAYLAND_CLIENT], [wayland-client wayland-cursor])
PKG_CHECK_MODULES(WAYLAND_PROTOCOLS, [wayland-protocols >= 1.2],
				     [ac_wayland_protocols_pkgdatadir=`$PKG_CONFIG --variable=pkgdatadir wayland-protocols`])
PKG_CHECK_MODULES(WAYLAND_PROTOCOLS, [wayland-protocols > 1.10],
		    [AC_SUBST(WAYLAND_PROTOCOLS_DATADIR, $ac_wayland_protocols_pkgdatadir)], [AC_SUBST(WAYLAND_PROTOCOLS_DATADIR, $PKG_CONFIG_SYSROOT_DIR$ac_wayland_protocols_pkgdatadir)])

WESTON_VERSION=`$PKG_CONFIG --modversion weston`
WESTON_PREFIX='-r0'
if test -d $PKG_CONFIG_SYSROOT_DIR/usr/src/debug/weston/$WESTON_VERSION$WESTON_PREFIX/weston-$WESTON_VERSION; then
	AC_SUBST(DEBUG_SRCDIR, usr/src/debug/weston/$WESTON_VERSION$WESTON_PREFIX/weston-$WESTON_VERSION)
else
	AC_SUBST(DEBUG_SRCDIR, usr/src/debug/weston/$WESTON_VERSION$WESTON_PREFIX/git)
fi

AC_PATH_PROG([wayland_scanner], [wayland-scanner])
if test x$wayland_scanner = x; then
        AC_MSG_ERROR([wayland-scanner is needed to compile weston])
fi


AC_CONFIG_FILES([Makefile])
AC_OUTPUT
