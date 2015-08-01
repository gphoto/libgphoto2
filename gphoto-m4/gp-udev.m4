dnl Determines whether UDEV code should be compiled.
dnl $1 contains shell code that returns 0 if all other prerequisites (like
dnl libusb) are available.
AC_DEFUN([GP_UDEV],[dnl
if test "x${udevscriptdir}" = "x"; then	udevscriptdir="\${libdir}/udev"; fi
AC_ARG_VAR([udevscriptdir],[Directory where udev scripts like check-ptp-camera will be installed])
AC_SUBST([udevscriptdir])
AM_CONDITIONAL([HAVE_UDEV],[if echo "$host"|grep -i linux >/dev/null ; then $1; fi ])
])dnl
