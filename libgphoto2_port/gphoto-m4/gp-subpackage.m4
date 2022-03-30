# gp-subpackage.m4 - whether to enable a subpackage            -*- Autoconf -*-
# serial 13
dnl | Increment the above serial number every time you edit this file.
dnl | When it finds multiple m4 files with the same name,
dnl | aclocal will use the one with the highest serial.
dnl
AC_DEFUN([GP_SUBPACKAGE],
[
AC_ARG_VAR([enable_subpackage_][$1], [enable subpackage ][$1][ (true or false)])
test "x${[enable_subpackage_][$1]}" = "x" && [enable_subpackage_][$1]=true
])dnl
dnl
dnl
dnl ####################################################################
dnl
dnl Local Variables:
dnl mode: autoconf
dnl End:
