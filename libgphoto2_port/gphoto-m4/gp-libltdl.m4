# gp-libltdl.m4 - check for libltdl (from libtool)             -*- Autoconf -*-
# serial 13
dnl | Increment the above serial number every time you edit this file.
dnl | When it finds multiple m4 files with the same name,
dnl | aclocal will use the one with the highest serial.
dnl
dnl Written by Hans Ulrich Niedermann
dnl LDFLAGS vs LIBS fix by Dan Nicholson
dnl
dnl ####################################################################
dnl GP_LIBLTDL
dnl ####################################################################
dnl
dnl We are using our own libltdl checks instead of AC_WITH_LTDL
dnl because we do not want to ship our own copy of libltdl.
dnl
dnl Look for external libltdl, not shipping internal libltdl.
AC_DEFUN([GP_LIBLTDL], [dnl
AC_ARG_VAR([LTDLINCL], [CFLAGS for compiling with libltdl])
AC_ARG_VAR([LIBLTDL],  [LIBS to add for linking against libltdl])
dnl
have_libltdl=no
AC_MSG_CHECKING([for libltdl])
AS_IF([test "x${LTDLINCL}${LIBLTDL}" != x], [dnl
  AC_MSG_RESULT([variables given explicitly])

  AC_MSG_CHECKING([LTDLINCL (given explicitly)])
  AC_MSG_RESULT([${LTDLINCL}])
  AC_MSG_CHECKING([LIBLTDL (given explicitly)])
  AC_MSG_RESULT([${LIBLTDL}])

  GP_LINK_LIBLTDL_IFELSE([dnl
    have_libltdl=yes
  ], [dnl
    GP_MSG_ERROR_LIBLTDL
  ])
], [dnl
  AC_MSG_RESULT([autodetecting])

  AS_VAR_IF([have_libltdl], [no], [dnl Try default location
    LTDLINCL=""
    LIBLTDL="-lltdl"
    AC_MSG_CHECKING([for libltdl at default location])
    GP_LINK_LIBLTDL_IFELSE([
      have_libltdl=yes
    ], [dnl
      AS_UNSET([LTDLINCL])
      AS_UNSET([LIBLTDL])
    ])
    AC_MSG_RESULT([$have_libltdl])
  ])

  dnl FreeBSD packages install to /usr/local
  dnl NetBSD  packages install to /usr/pkg
  dnl MacOSX  brew packages install to `brew --prefix`
  for gp_prefix in /usr/local /usr/pkg `brew --prefix 2>/dev/null`; do
    AS_IF([test -d "$gp_prefix"], [dnl
      AS_VAR_IF([have_libltdl], [no], [dnl
        LTDLINCL="-I${gp_prefix}/include"
        LIBLTDL="-L${gp_prefix}/lib -lltdl"
        AC_MSG_CHECKING([for libltdl at $gp_prefix])
        GP_LINK_LIBLTDL_IFELSE([
          have_libltdl=yes
          AC_MSG_RESULT([$have_libltdl])
          break
        ], [dnl
          AS_UNSET([LTDLINCL])
          AS_UNSET([LIBLTDL])
          AC_MSG_RESULT([$have_libltdl])
        ])
      ])
    ])
  done

  AS_VAR_IF([have_libltdl], [yes], [dnl
    AC_MSG_CHECKING([LTDLINCL (autodetected)])
    AC_MSG_RESULT([${LTDLINCL}])
    AC_MSG_CHECKING([LIBLTDL (autodetected)])
    AC_MSG_RESULT([${LIBLTDL}])
  ], [dnl
    GP_MSG_ERROR_LIBLTDL
  ])
])
])dnl
dnl
dnl
dnl ####################################################################
dnl GP_MSG_ERROR_LIBLTDL
dnl ####################################################################
dnl
dnl Print the error message when libltdl has not been found to work.
dnl
AC_DEFUN([GP_MSG_ERROR_LIBLTDL], [dnl
AC_MSG_ERROR([
${PACKAGE} requires libltdl (a part of libtool)

Please make sure that the proper development package is installed
(may be called libltdl-dev, libtool-ltdl-devel, libltdl, etc.) and if
the autodetection fails, that the LTDLINCL and LIBLTDL variables are
set properly on the configure command line.
])
])dnl
dnl
dnl
dnl ####################################################################
dnl GP_LINK_LIBLTDL_IFELSE
dnl ####################################################################
dnl
dnl Make sure we can actually compile and link against libltdl
dnl
AC_DEFUN([GP_LINK_LIBLTDL_IFELSE], [dnl
AC_LANG_PUSH([C])
saved_CPPFLAGS="$CPPFLAGS"
saved_LIBS="$LIBS"
CPPFLAGS="$CPPFLAGS $LTDLINCL"
LIBS="$LIBS $LIBLTDL"
AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <stdlib.h> /* for NULL */
#include <ltdl.h>   /* for lt_* */
]], [[
  int ret = lt_dlforeachfile("/usr/lib:/usr/local/lib", NULL, NULL);
  (void) ret;
]])], [$1], [$2])
CPPFLAGS="$saved_CPPFLAGS"
LIBS="$saved_LIBS"
AC_LANG_POP([C])
])dnl
dnl
dnl ####################################################################
dnl
dnl Local Variables:
dnl mode: autoconf
dnl End:
