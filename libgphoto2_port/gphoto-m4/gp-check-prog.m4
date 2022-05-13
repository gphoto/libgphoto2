# gp-check-prog.m4 - use user given or look for tool          -*- Autoconf -*-
# serial 1
dnl | Increment the above serial number every time you edit this file.
dnl | When it finds multiple m4 files with the same name,
dnl | aclocal will use the one with the highest serial.
dnl
dnl ####################################################################
dnl GP_CHECK_PROG([TOOLVAR], [tool], [description])
dnl
dnl Allow users to define TOOLVAR on the `configure` command line
dnl overriding all automatic checks. Otherwise, look for `tool` in
dnl `PATH` and set `TOOLVAR` to that value.
dnl
dnl Example:
dnl   GP_CHECK_PROG([SORT], [sort], [text file line sorting command])dnl
dnl
dnl   ./configure SORT="busybox sort"
dnl ####################################################################
dnl
AC_DEFUN([GP_CHECK_PROG], [dnl
AC_ARG_VAR([$1], [$2][ ][$3])dnl
AS_VAR_IF([$1], [], [dnl
AC_PATH_PROG([$1], [$2], [no])dnl
])
])dnl
dnl
dnl
dnl ####################################################################
dnl
dnl Local Variables:
dnl mode: autoconf
dnl End:
