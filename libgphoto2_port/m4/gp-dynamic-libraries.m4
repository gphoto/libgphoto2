AC_DEFUN([GP_DYNAMIC_LIBRARIES],[dnl
AC_REQUIRE([AC_PROG_LIBTOOL])
dnl ---------------------------------------------------------------------------
dnl Check for libtool: lt_dlforeachfile has been introduced in 
dnl                    libtool-1.4. However, there are still systems out
dnl                    there running libtool-1.3. For those, we will need
dnl                    dlopen. Note that on some systems (e.g. FreeBSD)
dnl                    -ldl isn't needed.
dnl ---------------------------------------------------------------------------
# $0
have_ltdl=false
ltdl_msg="no"
try_ltdl=false
AC_ARG_WITH([ltdl],
[AS_HELP_STRING([--with-ltdl],[use libtool ltdl])],
[
        if test x$withval = xyes; then
                try_ltdl=:
        fi
])
if $try_ltdl; then
        AC_CHECK_LIB([ltdl], [lt_dlforeachfile],[
                AC_CHECK_HEADER([ltdl.h],[
                        AC_DEFINE([HAVE_LTDL],1,[whether we use libltdl])
                        LTDL_LIBS="-lltdl"
                        ltdl_msg="yes"
                        have_ltdl=true
		])
	])
fi
GP_CONFIG_MSG([Use ltdl],[${ltdl_msg}])
if $have_ltdl; then :; else
        AC_CHECK_FUNC([dlopen],[],[
                AC_CHECK_LIB([dl], [dlopen], 
			[LTDL_LIBS="-ldl"],
			[AC_ERROR([
*** Could not determine how to handle
*** shared libraries:
***  - no libltdl
***  - no dlopen()
])
		])
	])
fi
AC_SUBST([LTDL_LIBS])
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
