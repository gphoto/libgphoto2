dnl doxygen related stuff
dnl look for tools
dnl define substitutions for Doxyfile.in
AC_DEFUN([GP_CHECK_DOXYGEN],[dnl
AC_REQUIRE([GP_CHECK_DOC_DIR])dnl
AC_PATH_PROG([DOT], [dot], [false])
AC_PATH_PROG([DOXYGEN], [doxygen], [false])
AM_CONDITIONAL([HAVE_DOXYGEN], [test "x$DOXYGEN" != "xfalse"])
AM_CONDITIONAL([HAVE_DOT], [test "x$DOT" != "xfalse"])
if test "x$DOT" != "xfalse"; then
	AC_SUBST([HAVE_DOT],[YES])
else
	AC_SUBST([HAVE_DOT],[NO])
fi
AC_SUBST([HTML_APIDOC_DIR], ["${PACKAGE_TARNAME}-api.html"])
AC_SUBST([DOXYGEN_OUTPUT_DIR], [doxygen-output])
AC_SUBST([HTML_APIDOC_INTERNALS_DIR], ["${PACKAGE_TARNAME}-internals.html"])
])dnl



