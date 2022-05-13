# gp-progs.m4 - look for certain well-known tools              -*- Autoconf -*-
# serial 15
dnl | Increment the above serial number every time you edit this file.
dnl | When it finds multiple m4 files with the same name,
dnl | aclocal will use the one with the highest serial.
dnl
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
GP_CHECK_PROG([CMP], [cmp], [file comparison command])dnl
])dnl
dnl
dnl
dnl ####################################################################
dnl
dnl ####################################################################
m4_pattern_forbid([GP_PROG_COMM])dnl
AC_DEFUN_ONCE([GP_PROG_COMM],[dnl
GP_CHECK_PROG([COMM], [comm], [line by line comparison command])dnl
])dnl
dnl
dnl
dnl ####################################################################
dnl
dnl ####################################################################
m4_pattern_forbid([GP_PROG_DIFF])dnl
AC_DEFUN_ONCE([GP_PROG_DIFF],[dnl
GP_CHECK_PROG([DIFF], [diff], [file comparison command])dnl
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
GP_CHECK_PROG([EXPR], [expr], [expression evaluation command])dnl
])dnl
dnl
dnl
dnl ####################################################################
dnl
dnl ####################################################################
m4_pattern_forbid([GP_PROG_SLEEP])dnl
AC_DEFUN_ONCE([GP_PROG_SLEEP],[dnl
GP_CHECK_PROG([SLEEP], [sleep], [delay command])dnl
AC_MSG_CHECKING([for sleep])
AC_MSG_RESULT([${SLEEP}])
])dnl
dnl
dnl
dnl ####################################################################
dnl
dnl ####################################################################
m4_pattern_forbid([GP_PROG_SORT])dnl
AC_DEFUN_ONCE([GP_PROG_SORT],[dnl
GP_CHECK_PROG([SORT], [sort], [text file line sorting command])dnl
])dnl
dnl
dnl
dnl ####################################################################
dnl
dnl ####################################################################
m4_pattern_forbid([^GP_PROG_TR\(])dnl
m4_pattern_forbid([^GP_PROG_TR$])dnl
AC_DEFUN_ONCE([GP_PROG_TR],[dnl
GP_CHECK_PROG([TR], [tr], [string character translation command])dnl
])dnl
dnl
dnl
dnl ####################################################################
dnl
dnl ####################################################################
m4_pattern_forbid([^GP_PROG_UNIQ\(])dnl
m4_pattern_forbid([^GP_PROG_UNIQ$])dnl
AC_DEFUN_ONCE([GP_PROG_UNIQ],[dnl
GP_CHECK_PROG([UNIQ], [uniq], [sorted file unification command])dnl
])dnl
dnl
dnl
dnl ####################################################################
dnl
dnl Local Variables:
dnl mode: autoconf
dnl End:
