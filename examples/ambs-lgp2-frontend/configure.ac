AC_PREREQ([2.69])
AC_INIT([automake buildsystem libgphoto2 frontend],
        [0.0.1],
	[https://github.com/gphoto/libgphoto2],
	[ambs-lgp2-frontend])
AC_CONFIG_SRCDIR([main.c])
AC_CONFIG_HEADERS([auto-config.h])
AC_CONFIG_AUX_DIR([auto-aux])
AC_CONFIG_MACRO_DIR([auto-m4])

AM_INIT_AUTOMAKE([
  -Wall
  foreign
  1.16
  subdir-objects
])

AM_SILENT_RULES([no])

AC_PROG_CC


m4_pattern_forbid([^PKG_CHECK_MODULES])dnl
PKG_CHECK_MODULES([LIBGPHOTO2], [libgphoto2])


AC_CONFIG_FILES([
  Makefile
])
AC_OUTPUT
