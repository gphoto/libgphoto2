AC_DEFUN([GP_DYNAMIC_LIBRARIES],[dnl
dnl We require either of those
dnl AC_REQUIRE([AC_LIBTTDL_INSTALLABLE])dnl
dnl AC_REQUIRE([AC_LIBLTDL_CONVENIENCE])dnl
AC_REQUIRE([AC_LIBTOOL_DLOPEN])dnl
AC_REQUIRE([AC_PROG_LIBTOOL])dnl
dnl ---------------------------------------------------------------------------
dnl Check for libltdl:
dnl  - lt_dlforeachfile has been introduced in libtool-1.4.
dnl  - However, there are still systems out there running libtool-1.3.
dnl    For those, we will use our shipped libltdl. This has the welcome
dnl    side effect that we don't have to distinguish between libltdl 1.3 with
dnl    and without the notorious segfault bug.
dnl  - FIXME: In case we're using our own version, we have to check whether
dnl           -ldl is required?
dnl ---------------------------------------------------------------------------
# $0
ltdl_msg="no (not found or too old)"
have_ltdl=false
LIBS_save="$LIBS"
LIBS="$LIBLTDL"
AC_CHECK_LIB([ltdl], [lt_dlforeachfile],[
	CPPFLAGS_save="$CPPFLAGS"
	CPPFLAGS="$LTDLINCL"
	AC_CHECK_HEADER([ltdl.h],[
		AC_DEFINE([HAVE_LTDL],1,[whether we use libltdl])
		ltdl_msg="yes (from system)"
		have_ltdl=:
	])
	CPPFLAGS="$CPPFLAGS_save"
])
LIBS="$LIBS_save"
if "$have_ltdl"; then :; else
	AC_MSG_CHECKING([for included libltdl])
	if test -d "$srcdir/libltdl"; then
		LIBLTDL="\$(top_builddir)/libltdl/libltdlc.la"
		LTDLINCL="-I\$(top_srcdir)/libltdl"
		have_ltdl=:
		ltdl_msg="yes (included)"
		AC_MSG_RESULT([yes])
	else
		AC_MSG_RESULT([no])
		AC_MSG_ERROR([
*** Could not detect or configure libltdl.
])
	fi
fi
GP_CONFIG_MSG([libltdl],["${ltdl_msg}"])
])dnl
dnl
dnl ####################################################################
dnl
dnl Please do not remove this:
dnl filetype: 2b993145-3256-47b4-84fd-ec4dcdf4fdf9
dnl I use this to find all the different instances of this file which 
dnl are supposed to be synchronized.
dnl
dnl Local Variables:
dnl mode: autoconf
dnl End:
