AC_DEFUN([GP_SUBPACKAGE],
[
AC_ARG_VAR([enable_subpackage_][$1], [enable subpackage ][$1][ (true or false)])
test "x${[enable_subpackage_][$1]}" = "x" && [enable_subpackage_][$1]=true
])dnl
