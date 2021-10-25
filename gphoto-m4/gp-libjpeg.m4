dnl ####################################################################
dnl GP_LIBJPEG
dnl ####################################################################
dnl
dnl   * If --without-jpeg or --with-jpeg=no is given, build without
dnl     libjpeg support.
dnl   * If not explicitly disabled by --without-jpeg, autodetect libjpeg.
dnl       * If any of LIBJPEG_(CFLAGS|LIBS) is explicitly given, try
dnl         compile+link using that.
dnl          * If compile+link works, use that.
dnl          * If compile+link fails, abort with error message.
dnl       * If none of LIBJPEG_(CFLAGS|LIBS) are explicitly given, try
dnl         pkg-config to find libjpeg.pc.
dnl          * If libjpeg.pc has been found, try compile+link.
dnl              * If compile+link works, use that.
dnl              * If compile+link fails, build without libjpeg.
dnl          * If libjpeg.pc has not been found, try default location.
dnl              * If compile+link works, use that.
dnl              * If compile+link fails, build without libjpeg.
dnl
AC_DEFUN([GP_LIBJPEG], [dnl
dnl
AC_MSG_CHECKING([whether to build with libjpeg])
AC_ARG_WITH([jpeg], [dnl
  AS_HELP_STRING([--without-jpeg],
                 [Build without libjpeg (default: with libjpeg)])
], [dnl just keep the with-jpeg however it is given
  AS_VAR_IF([with_jpeg], [no], [], [dnl
    AC_MSG_ERROR([
Unhandled value given to --with-jpeg / --without-jpeg: '$with_jpeg'
])
  ])
], [dnl
  with_jpeg=autodetect
])
AC_MSG_RESULT([$with_jpeg])
dnl
AS_VAR_IF([with_jpeg], [no], [dnl Not using libjpeg, so no checks are needed
  # libjpeg explictly disabled from command line
  GP_CONFIG_MSG([JPEG mangling support],
                [no (disabled by --without-jpeg)])
], [dnl
  have_libjpeg=no

  AC_MSG_CHECKING([for libjpeg via variables])
  AS_IF([test "x$LIBJPEG_LIBS$LIBJPEG_CFLAGS" != x], [dnl
    GP_LINK_LIBJPEG_IFELSE([
      AC_MSG_RESULT([found and works])
      have_libjpeg=yes
    ], [dnl
      AC_MSG_RESULT([found but fails to link])
      AC_MSG_ERROR([
libjpeg not found despite LIBJPEG_CFLAGS and/or LIBJPEG_LIBS being set.
])
    ])
  ], [dnl
    AC_MSG_RESULT([no])
  ])

  AS_VAR_IF([have_libjpeg], [no], [dnl
    PKG_CHECK_MODULES([LIBJPEG], [libjpeg], [dnl
      AC_MSG_CHECKING([linking with libjpeg works])
      GP_LINK_LIBJPEG_IFELSE([dnl
        have_libjpeg=yes
        AC_MSG_RESULT([yes])
      ], [dnl
        AC_MSG_RESULT([no])
      ])
    ], [dnl
      LIBJPEG_LIBS="-ljpeg"
      AC_MSG_CHECKING([for libjpeg at default location])
      GP_LINK_LIBJPEG_IFELSE([dnl
        have_libjpeg=yes
        AC_MSG_RESULT([yes])
      ], [dnl
        AC_MSG_RESULT([no])
        AS_UNSET([LIBJPEG_LIBS])
      ])
    ])
  ])

  AS_VAR_IF([have_libjpeg], [no], [dnl
    GP_CONFIG_MSG([JPEG mangling support],
                  [${have_libjpeg} (requires libjpeg)])
  ], [dnl
    AC_DEFINE([HAVE_LIBJPEG], [1],
              [define if building with libjpeg])
    GP_CONFIG_MSG([JPEG mangling support],
                  [${have_libjpeg}])
  ])
])
])dnl
dnl
dnl
dnl ####################################################################
dnl GP_LINK_LIBJPEG_IFELSE([if-true], [if-false])
dnl Make sure we can actually compile and link against libjpeg.
dnl ####################################################################
dnl
AC_DEFUN([GP_LINK_LIBJPEG_IFELSE], [dnl
AC_LANG_PUSH([C])
saved_CPPFLAGS="$CPPFLAGS"
saved_LIBS="$LIBS"
CPPFLAGS="$CPPFLAGS $LIBJPEG_CFLAGS"
LIBS="$LIBS $LIBJPEG_LIBS"
AC_LINK_IFELSE([AC_LANG_SOURCE([[
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
/* jpeglib.h fails to include all required system headers, so jpeglib.h
 * must be included after stddef.h (size_t) and stdio.h (FILE).
 */
#include <jpeglib.h>

int main(int argc, char **argv)
{
  j_decompress_ptr cinfo = NULL;
  (void) argc;
  (void) argv;
  /* Running this will give a segfault */
  if (jpeg_start_decompress(cinfo)) {
    printf("true\n");
  } else {
    printf("false\n");
  }
  return 0;
}
]])], [$1], [$2])
CPPFLAGS="$saved_CPPFLAGS"
LIBS="$saved_LIBS"
AC_LANG_POP([C])
])dnl
dnl
dnl
dnl ####################################################################
dnl
dnl Local Variables:
dnl mode: autoconf
dnl End:
