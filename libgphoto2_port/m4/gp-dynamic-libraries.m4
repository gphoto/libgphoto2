AC_DEFUN([GP_DYNAMIC_LIBRARIES],[dnl
dnl We require either of those
dnl AC_REQUIRE([AC_LIBTTDL_INSTALLABLE])dnl
dnl AC_REQUIRE([AC_LIBLTDL_CONVENIENCE])dnl
AC_REQUIRE([AC_LIBTOOL_DLOPEN])dnl
AC_REQUIRE([AC_PROG_LIBTOOL])dnl
dnl ---------------------------------------------------------------------------
dnl Check for libtool: lt_dlforeachfile has been introduced in 
dnl                    libtool-1.4. However, there are still systems out
dnl                    there running libtool-1.3. For those, we will need
dnl                    dlopen. Note that on some systems (e.g. FreeBSD)
dnl                    -ldl isn't needed.
dnl ---------------------------------------------------------------------------
# $0
LDFLAGS_save="$LDFLAGS"
LDFLAGS="$LIBLTDL"
ltdl_msg="no (probably using built-in)"
AC_CHECK_LIB([ltdl], [lt_dlforeachfile],[
	CPPFLAGS_save="$CPPFLAGS"
	CPPFLAGS="$LTDLINCL"
	AC_CHECK_HEADER([ltdl.h],[
		AC_DEFINE([HAVE_LTDL],1,[whether we use libltdl])
		ltdl_msg="yes"
		have_ltdl=true
	])
	CPPFLAGS="$CPPFLAGS_save"
])
LDFLAGS="$LDFLAGS_save"
GP_CONFIG_MSG([Use ltdl (???)],[${ltdl_msg}])
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
