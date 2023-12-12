#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([weston-simple-egl], [1.0.0], [https://www.renesas.com/])
AC_CONFIG_SRCDIR([simple-egl.c])
AC_CONFIG_HEADERS([config.h])

# Initialize Automake
AM_INIT_AUTOMAKE([foreign dist-bzip2])

# Initialize libtool
AC_PROG_LIBTOOL

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_INT32_T
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T

AC_ARG_ENABLE(egl, [  --disable-egl],, enable_egl=yes)
AM_CONDITIONAL(ENABLE_EGL, test x$enable_egl = xyes)
if test x$enable_egl = xyes; then
        AC_DEFINE([ENABLE_EGL], [1], [Build Weston with EGL support])
        PKG_CHECK_MODULES([SIMPLE_EGL_CLIENT], [egl glesv2 wayland-client wayland-egl wayland-cursor])
fi

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([gettimeofday])

# check dependencies
PKG_CHECK_MODULES([GLES], [egl glesv2])
PKG_CHECK_MODULES(SIMPLE_CLIENT, [wayland-egl wayland-client wayland-cursor])
PKG_CHECK_MODULES(WAYLAND_PROTOCOLS, [wayland-protocols >= 1.2],
				     [ac_wayland_protocols_pkgdatadir=`$PKG_CONFIG --variable=pkgdatadir wayland-protocols`])
PKG_CHECK_MODULES(WAYLAND_PROTOCOLS, [wayland-protocols > 1.10],
		    [AC_SUBST(WAYLAND_PROTOCOLS_DATADIR, $ac_wayland_protocols_pkgdatadir)], [AC_SUBST(WAYLAND_PROTOCOLS_DATADIR, $PKG_CONFIG_SYSROOT_DIR$ac_wayland_protocols_pkgdatadir)])

AC_PATH_PROG([wayland_scanner], [wayland-scanner])
if test x$wayland_scanner = x; then
        AC_MSG_ERROR([wayland-scanner is needed to compile weston])
fi

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
