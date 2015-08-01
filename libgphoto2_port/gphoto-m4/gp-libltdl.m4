dnl Written by Hans Ulrich Niedermann
dnl LDFLAGS vs LIBS fix by Dan Nicholson
dnl 
dnl We are using our own libltdl checks instead of AC_WITH_LTDL
dnl because we do not want to ship our own copy of libltdl any more.
dnl Not shipping libltdl makes it possible to ditch our own autogen.sh
dnl and relying on standard autoconf's "autoreconf".
dnl
dnl Look for external libltdl, not shipping internal libltdl.
AC_DEFUN([GP_LIB_LTDL],[dnl
AC_ARG_VAR([LTDLINCL],[CFLAGS for compiling with libltdl])
AC_ARG_VAR([LIBLTDL],[LIBS to add for linking against libltdl])
if test "x${LTDLINCL}${LIBLTDL}" = "x"; then
AC_CHECK_HEADER([ltdl.h],
[dnl
AC_CHECK_LIB([ltdl], [lt_dlinit],[dnl
LTDLINCL=""
LIBLTDL="-lltdl"
AC_DEFINE([HAVE_LTDL],[1],[Whether libltdl (of libtool fame) is present])
],[dnl
AC_MSG_ERROR([
$PACKAGE requires the ltdl library, included with libtool

Please make sure that the proper development package is installed
(libltdl-dev, libtool-ltdl-devel, etc.)
])[]dnl
])dnl
])
else
	AC_MSG_CHECKING([for libltdl flags])
	AC_MSG_RESULT([set explicitly: ${LTDLINCL} ${LIBLTDL}])
fi
AC_SUBST([LTDLINCL])
AC_SUBST([LIBLTDL])
dnl
dnl Make sure we can actually compile and link against libltdl
AC_LANG_PUSH([C])
AC_MSG_CHECKING([that we can compile and link with libltdl])
saved_CPPFLAGS="$CPPFLAGS"
saved_LIBS="$LIBS"
CPPFLAGS="$CPPFLAGS $LTDLINCL"
LIBS="$LIBS $LIBLTDL"
AC_LINK_IFELSE([AC_LANG_PROGRAM([dnl
#include <stdlib.h> /* for NULL */
#include <ltdl.h>   /* for lt_* */
],[dnl
int ret = lt_dlforeachfile("/usr/lib:/usr/local/lib", NULL, NULL);
])], [AC_MSG_RESULT([yes])], [dnl
AC_MSG_RESULT([no])
AC_MSG_ERROR([cannot compile and link against libltdl
${PACKAGE_TARNAME} requires libltdl (the libtool dl* library),
but cannot compile and link against it.
Aborting.
])
])
CPPFLAGS="$saved_CPPFLAGS"
LIBS="$saved_LIBS"
AC_LANG_POP
])dnl
