dnl
dnl
dnl
AC_DEFUN([GP_INTERNAL_DOCS],[dnl
# Whether to enable the internal docs build.
#
# This takes quite some time due to the generation of lots of call
# graphs, so it is disabled by default.
set_enable_internal_docs=no
AC_ARG_ENABLE([internal-docs], [dnl
AS_HELP_STRING([--enable-internal-docs], 
[Build internal code docs if doxygen available])], [dnl
dnl If either --enable-foo nor --disable-foo were given, execute this.
  if   test "x$enableval" = xno \
    || test "x$enableval" = xoff \
    || test "x$enableval" = xfalse; 
  then
    set_enable_internal_docs=no
  elif test "x$enableval" = xyes \
    || test "x$enableval" = xon \
    || test "x$enableval" = xtrue
  then
    set_enable_internal_docs=yes
  fi
])
AC_MSG_CHECKING([whether to create internal code docs])
AC_MSG_RESULT([${set_enable_internal_docs}])
AM_CONDITIONAL([ENABLE_INTERNAL_DOCS], [test "x${set_enable_internal_docs}" = "xyes"])
])dnl
