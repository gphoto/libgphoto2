dnl ####################################################################
dnl doxygen related stuff
dnl look for tools
dnl define substitutions for Doxyfile.in
dnl ####################################################################
dnl
AC_DEFUN([GP_CHECK_DOXYGEN], [dnl
dnl
AC_ARG_VAR([DOT],          [graphviz dot directed graph drawing command])
AC_PATH_PROG([DOT],        [dot], [no])
AM_CONDITIONAL([HAVE_DOT], [test "x$DOT" != xno])
dnl
AC_ARG_VAR([DOXYGEN],          [software documentation generator command])
AC_PATH_PROG([DOXYGEN],        [doxygen], [no])
AM_CONDITIONAL([HAVE_DOXYGEN], [test "x$DOXYGEN" != xno])
dnl
dnl Substitutions for Doxyfile.in
AM_COND_IF([HAVE_DOT],
           [AC_SUBST([HAVE_DOT], [YES])],
           [AC_SUBST([HAVE_DOT], [NO])])
AC_SUBST([HTML_APIDOC_DIR], ["${PACKAGE_TARNAME}-api.html"])
AC_SUBST([DOXYGEN_OUTPUT_DIR], [doxygen-output])
AC_SUBST([HTML_APIDOC_INTERNALS_DIR], ["${PACKAGE_TARNAME}-internals.html"])
dnl
])dnl
dnl
dnl
dnl ####################################################################
dnl
dnl Local Variables:
dnl mode: autoconf
dnl End:
