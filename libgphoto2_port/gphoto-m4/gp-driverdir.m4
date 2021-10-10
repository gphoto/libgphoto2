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
AC_MSG_CHECKING([where to install ][$3])
AC_ARG_WITH([$1], [AS_HELP_STRING(
	[--with-][$1][=<path>],
	[install ][$3][ in directory <path>])dnl
], [
	$1="$withval"
], [
	$1="\${libdir}/${PACKAGE_TARNAME}/${PACKAGE_VERSION}"
])
AC_MSG_RESULT([${$1}])
AC_SUBST([$1])
AM_CPPFLAGS="$AM_CPPFLAGS -D$2=\\\"${$1}\\\""
])dnl
dnl
dnl
dnl ####################################################################
dnl
dnl Local Variables:
dnl mode: autoconf
dnl End:
