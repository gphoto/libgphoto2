# gp-driverdir.m4 - define install dirs for camlibs and iolibs -*- Autoconf -*-
# serial 13
dnl | Increment the above serial number every time you edit this file.
dnl | When it finds multiple m4 files with the same name,
dnl | aclocal will use the one with the highest serial.
dnl
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
AC_ARG_VAR([$1],
           [where to install ][$3][ (default: ${libdir}/${PACKAGE_TARNAME}/${PACKAGE_VERSION})])
AS_VAR_IF([$1], [], [dnl
	$1="\${libdir}/${PACKAGE_TARNAME}/${PACKAGE_VERSION}"
	AC_MSG_RESULT([${$1} (default)])
], [dnl
	AC_MSG_RESULT([${$1} (set explicitly)])
])

dnl If you see this after 2022-12-31, please remove the following
dnl section, uncomment the one after, and send a pull request.
AC_MSG_CHECKING([for deprecated --with-$1 argument])
AC_ARG_WITH([$1], [AS_HELP_STRING(
	[--with-][$1][=<path>],
	[deprecated (use ][$1][= variable instead)])dnl
], [dnl
	AS_VAR_IF([$1], [], [dnl
		$1="$withval"
		AC_MSG_RESULT([${$1} (from DEPRECATED --with-$1)])
	], [dnl
		AS_VAR_IF([$1], ["$withval"], [dnl
			# Nothing to do, $1 has already been set to this value.
		], [dnl
			AC_MSG_RESULT([${withval} (differs from $1 value)])
			AC_MSG_ERROR([
If both the $1= variable and the DEPRECATED --with-$1= argument
are used, their value MUST be the same.
])
		])
	])
], [dnl
	AC_MSG_RESULT([not used (very good)])
])

dnl If you see this after 2022-12-31, please uncomment the following
dnl section, remove the previous one, and send a pull request.
dnl
dnl AC_ARG_WITH([$1], [AS_HELP_STRING([--with-][$1][=<path>],
dnl                                   [DEPRECATED (use camlibdir= variable instead)])dnl
dnl ], [dnl
dnl 	AC_MSG_ERROR([
dnl The --with-$1= argument is DEPRECATED.
dnl
dnl Use the $1= variable instead.
dnl ])
dnl ])

AC_SUBST([$1])

AC_ARG_VAR([DEFAULT_][$2],
           [default location to look for ][$3][ at runtime (if not given, use ${$1})])
AC_MSG_CHECKING([default location to look for $3])
AS_VAR_IF([DEFAULT_][$2], [], [dnl
	DEFAULT_$2="\${$1}"
	AC_MSG_RESULT([value of $1 (default)])
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
