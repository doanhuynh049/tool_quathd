#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([render_node_map], [1.0.0], [https://www.renesas.com/])
AC_CONFIG_SRCDIR([render_node_map.c])
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

# Checks for library functions.
AC_FUNC_MALLOC

# check dependencies
PKG_CHECK_MODULES([DRM], [libdrm])
PKG_CHECK_MODULES([KMS], [libkms])
PKG_CHECK_MODULES([LIBGBM], [gbm >= 10.3.0])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
