# gp-libjpeg.m4 - check for libjpeg                            -*- Autoconf -*-
# serial 13
dnl | Increment the above serial number every time you edit this file.
dnl | When it finds multiple m4 files with the same name,
dnl | aclocal will use the one with the highest serial.
dnl
dnl ####################################################################
dnl GP_LIBJPEG
dnl ####################################################################
dnl
dnl Define the libjpeg related compile and linker flags depending on
dnl configure arguments and availability on the system.
dnl
dnl   * If --without-jpeg or --with-jpeg=no is given, build without
dnl     libjpeg support.
dnl
dnl   * If --with-jpeg=auto or --with-jpeg=autodetect is given,
dnl     autodetect libjpeg:
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
dnl   * If --with-jpeg or --with-jpeg=yes is given, autodetect as
dnl     described above, but abort with an error message if libjpeg
dnl     could not be found.
dnl
dnl   * If neither --with-jpeg nor --without-jpeg are explicitly given,
dnl     run the above autodetect sequence.
dnl
AC_DEFUN([GP_LIBJPEG], [dnl
dnl
AC_MSG_CHECKING([whether to build with libjpeg])
AC_ARG_WITH([jpeg],
            [AS_HELP_STRING([--without-jpeg],
                            [Build without libjpeg (default: autodetect)])],
            [dnl Normalize --with-jpeg=ARG argument value
  AS_CASE([$with_jpeg],
    [autodetect], [with_jpeg=auto],
  )
], [dnl Default value
  with_jpeg=auto
])
dnl
AC_MSG_RESULT([$with_jpeg])
dnl
AS_CASE([$with_jpeg],
[no], [
  # libjpeg explicitly disabled from command line
  GP_CONFIG_MSG([JPEG mangling support],
                [no (disabled by --without-jpeg)])
],
[auto|yes], [
  GP_LIBJPEG_AUTODETECT

  AS_VAR_IF([have_libjpeg], [no], [dnl
    AS_VAR_IF([with_jpeg], [yes], [dnl
      AC_MSG_ERROR([
libjpeg has been requested explicitly (--with-jpeg=yes), but could not be
found and made to work.
])
    ], [dnl
      GP_CONFIG_MSG([JPEG mangling support],
                    [${have_libjpeg} (could not find working libjpeg)])
    ])
  ], [dnl
    AC_DEFINE([HAVE_LIBJPEG], [1],
              [define if building with libjpeg])
    GP_CONFIG_MSG([JPEG mangling support],
                  [${have_libjpeg}])
  ])
],
[AC_MSG_ERROR([
Unhandled value given to --with-jpeg: ${with_jpeg}

To allow the auto-detection of libjpeg, set up the pkg-config related
environment variables (PKG_CONFIG, PKG_CONFIG_PATH, PKG_CONFIG_LIBDIR)
or have libjpeg installed in the standard location to include from and
link to.

To force specific compile and link flags for libjpeg, set the
environment variables LIBJPEG_CFLAGS and $LIBJPEG_LIBS accordingly.
])
])
])dnl
dnl
dnl
dnl ####################################################################
dnl GP_LIBJPEG_AUTODETECT
dnl
dnl Do the actual autodetection of libjpeg, setting
dnl
dnl   have_libjpeg=yes
dnl     If libjpeg has been found and can be both compiled with and
dnl     linked against.
dnl
dnl   have_libjpeg=no
dnl     If libjpeg has not been found or cannot be compiled with or
dnl     cannot be linked against.
dnl
dnl Used once: By GP_LIBJPEG to make the GP_LIBJPEG code more readable.
dnl ####################################################################
dnl
AC_DEFUN([GP_LIBJPEG_AUTODETECT], [dnl
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
