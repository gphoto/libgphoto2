dnl ###################################################################
dnl Driver directory (camlibdir or iolibdir)
dnl ###################################################################
dnl
dnl Usage:
dnl    GP_DRIVERDIR([camlibdir], [CAMLIBS], [camlibs])
dnl    GP_DRIVERDIR([iolibdir],  [IOLIBS],  [iolibs])
dnl
dnl ###################################################################
dnl
AC_DEFUN([GP_DRIVERDIR], [dnl
AC_MSG_CHECKING([where to install ][$3][ ($1)])
AC_ARG_WITH([$1], [AS_HELP_STRING(
	[--with-][$1][=<path>],
	[install ][$3][ in directory <path>])dnl
], [dnl
	$1="$withval"
	AC_MSG_RESULT([${$1} (from --with-$1)])
], [dnl
	$1="\${libdir}/${PACKAGE_TARNAME}/${PACKAGE_VERSION}"
	AC_MSG_RESULT([${$1} (default)])
])
AC_SUBST([$1])

AC_ARG_VAR([DEFAULT_][$2],
           [default location to look for ][$3][ at runtime (using ${$1} if not given)])
AS_VAR_IF([DEFAULT_][$2], [], [dnl
	DEFAULT_$2="\${$1}"
	AC_MSG_RESULT([value of camlibdir (default)])
], [dnl
	AC_MSG_RESULT([${DEFAULT_$2} (set explicitly)])
])
AM_CPPFLAGS="$AM_CPPFLAGS -D$2=\\\"${DEFAULT_$2}\\\""
])dnl
dnl
dnl
dnl ####################################################################
dnl
dnl Local Variables:
dnl mode: autoconf
dnl End:
