dnl ####################################################################
dnl GP_SLEEP(delay_in_whole_seconds)
dnl   If the SLEEP variable is set to "no" or empty or is unset,
dnl   do not sleep.
dnl   If the SLEEP variable is set to something else, run that something
dnl   else with the given delay value.
dnl ####################################################################
AC_DEFUN_ONCE([_GP_SLEEP_INIT], [dnl
AC_REQUIRE([GP_PROG_SLEEP])dnl
AS_IF([test "x$SLEEP" != "x" && test "x$SLEEP" != "xno"], [dnl
gp_sleep=[$]SLEEP
], [dnl
gp_sleep=:
])
])dnl
dnl
dnl
AC_DEFUN([GP_SLEEP], [dnl
AC_REQUIRE([_GP_SLEEP_INIT])dnl
$gp_sleep $1
])dnl
dnl
dnl
dnl ####################################################################
dnl
dnl Local Variables:
dnl mode: autoconf
dnl End:
