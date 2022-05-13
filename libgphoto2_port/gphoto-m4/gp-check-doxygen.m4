# gp-check-doxygen.m4 - check for doxygen tool                 -*- Autoconf -*-
# serial 14
dnl | Increment the above serial number every time you edit this file.
dnl | When it finds multiple m4 files with the same name,
dnl | aclocal will use the one with the highest serial.
dnl
dnl ####################################################################
dnl doxygen related stuff
dnl look for tools
dnl define substitutions for Doxyfile.in
dnl ####################################################################
dnl
AC_DEFUN([GP_CHECK_DOXYGEN], [dnl
dnl
GP_CHECK_PROG([DOT], [dot], [graphviz directed graph drawing command])
dnl
AM_CONDITIONAL([HAVE_DOT], [test "x$DOT" != xno])
AC_MSG_CHECKING([HAVE_DOT])
AM_COND_IF([HAVE_DOT], [dnl
  gp_config_msg_dot=", using dot (${DOT})"
  AC_MSG_RESULT([yes ($DOT)])
], [dnl
  gp_config_msg_dot=", not using dot"
  AC_MSG_RESULT([no])
])
dnl
dnl
GP_CHECK_PROG([DOXYGEN], [doxygen], [software documentation generator command])
dnl
AM_CONDITIONAL([HAVE_DOXYGEN], [test "x$DOXYGEN" != xno])
AC_MSG_CHECKING([HAVE_DOXYGEN])
AM_COND_IF([HAVE_DOXYGEN], [dnl
  AC_MSG_RESULT([yes ($DOXYGEN)${gp_config_msg_dot})])
  GP_CONFIG_MSG([build doxygen docs], [yes])
], [dnl
  AC_MSG_RESULT([no])
  GP_CONFIG_MSG([build doxygen docs], [no])
])
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
