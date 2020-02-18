dnl ####################################################################
dnl Find a number of common programs, but allow setting a variable
dnl to use a specific implementation.
dnl ####################################################################
dnl
dnl
dnl ####################################################################
dnl
dnl ####################################################################
m4_pattern_forbid([GP_PROG_CMP])dnl
AC_DEFUN_ONCE([GP_PROG_CMP],[dnl
AC_ARG_VAR([CMP], [cmp file comparison command])dnl
AC_PATH_PROG([CMP], [cmp])dnl
])dnl
dnl
dnl
dnl ####################################################################
dnl
dnl ####################################################################
m4_pattern_forbid([GP_PROG_COMM])dnl
AC_DEFUN_ONCE([GP_PROG_COMM],[dnl
AC_ARG_VAR([COMM], [comm line by line comparison command])dnl
AC_PATH_PROG([COMM], [comm])dnl
])dnl
dnl
dnl
dnl ####################################################################
dnl
dnl ####################################################################
m4_pattern_forbid([GP_PROG_DIFF])dnl
AC_DEFUN_ONCE([GP_PROG_DIFF],[dnl
AC_ARG_VAR([DIFF], [diff file comparison command])dnl
AC_PATH_PROG([DIFF], [diff])dnl
dnl
dnl DIFF_MAYBE_U only contains -u for readability, so if diff does not
dnl support -u, we can still just run DIFF without -u.
AC_MSG_CHECKING([whether DIFF supports -u])
:>gp-empty-file-a
:>gp-empty-file-b
AS_IF([diff -u gp-empty-file-a gp-empty-file-b], [dnl
       AC_MSG_RESULT([yes])
       DIFF_MAYBE_U="${DIFF} -u"
], [dnl
       AC_MSG_RESULT([no])
       DIFF_MAYBE_U="${DIFF}"
])
rm -f gp-empty-file-a gp-empty-file-b
AC_SUBST([DIFF_MAYBE_U])
])dnl
dnl
dnl
dnl ####################################################################
dnl
dnl ####################################################################
m4_pattern_forbid([GP_PROG_EXPR])dnl
AC_DEFUN_ONCE([GP_PROG_EXPR],[dnl
AC_ARG_VAR([EXPR], [expr expression evaluation command])dnl
AC_PATH_PROG([EXPR], [expr])dnl
])dnl
dnl
dnl
dnl ####################################################################
dnl
dnl ####################################################################
m4_pattern_forbid([GP_PROG_SLEEP])dnl
AC_DEFUN_ONCE([GP_PROG_SLEEP],[dnl
AC_ARG_VAR([SLEEP], [sleep delay command])dnl
AC_MSG_CHECKING([whether to sleep])
AS_VAR_IF([SLEEP], [no], [dnl
  AC_MSG_RESULT([no])
], [dnl
  AC_MSG_RESULT([yes])
  AC_PATH_PROG([SLEEP], [sleep])dnl
])dnl
])dnl
dnl
dnl
dnl ####################################################################
dnl
dnl ####################################################################
m4_pattern_forbid([GP_PROG_SORT])dnl
AC_DEFUN_ONCE([GP_PROG_SORT],[dnl
AC_ARG_VAR([SORT], [sort text file line sorting command])dnl
AC_PATH_PROG([SORT], [sort])dnl
])dnl
dnl
dnl
dnl ####################################################################
dnl
dnl ####################################################################
m4_pattern_forbid([^GP_PROG_TR\(])dnl
m4_pattern_forbid([^GP_PROG_TR$])dnl
AC_DEFUN_ONCE([GP_PROG_TR],[dnl
AC_ARG_VAR([TR], [tr string character translation command])dnl
AC_PATH_PROG([TR], [tr])dnl
])dnl
dnl
dnl
dnl ####################################################################
dnl
dnl ####################################################################
m4_pattern_forbid([^GP_PROG_UNIQ\(])dnl
m4_pattern_forbid([^GP_PROG_UNIQ$])dnl
AC_DEFUN_ONCE([GP_PROG_UNIQ],[dnl
AC_ARG_VAR([UNIQ], [uniq sorted file uniquification command])dnl
AC_PATH_PROG([UNIQ], [uniq])dnl
])dnl
dnl
dnl
dnl ####################################################################
dnl
dnl Local Variables:
dnl mode: autoconf
dnl End:
