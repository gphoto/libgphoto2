# gp-sleep.m4 - look for the sleep(1) tool                     -*- Autoconf -*-
# serial 14
dnl | Increment the above serial number every time you edit this file.
dnl | When it finds multiple m4 files with the same name,
dnl | aclocal will use the one with the highest serial.
dnl
dnl ####################################################################
dnl GP_SLEEP(delay_in_whole_seconds)
dnl   * If the SLEEP variable is set to "no" or empty or is unset,
dnl     do not sleep.
dnl   * If stdout (FD 1) is not a TTY (!isatty(1)), do not sleep.
dnl   * Otherwise, run ${SLEEP} with the given delay value argument.
dnl ####################################################################
dnl
dnl ####################################################################
dnl GP_SLEEP_IF_TTY(delay_in_whole_seconds)
dnl   * If the SLEEP variable is set to "no" or empty or is unset,
dnl     do not sleep.
dnl   * If stdout (FD 1) is not a TTY (!isatty(1)), do not sleep.
dnl   * If tty -t is not present and therefore fails, we do not lose
dnl     much: Just that interactive users do not benefit from seeing the
dnl     warning a bit longer.
dnl   * Otherwise, run ${SLEEP} with the given delay value argument.
dnl ####################################################################
dnl
AC_DEFUN_ONCE([_GP_SLEEP_INIT], [dnl
AC_REQUIRE([GP_PROG_SLEEP])dnl
AC_MSG_CHECKING([whether configure script should sleep])
AS_IF([test "x$SLEEP" != x && test "x$SLEEP" != xno], [dnl
  AC_MSG_RESULT([yes])
  gp_sleep=[$]SLEEP
], [dnl
  AC_MSG_RESULT([no])
  gp_sleep=:
])
])dnl
dnl
dnl
AC_DEFUN([GP_SLEEP], [dnl
AC_REQUIRE([_GP_SLEEP_INIT])dnl
[$]gp_sleep $1
])dnl
dnl
AC_DEFUN([GP_SLEEP_IF_TTY], [dnl
AC_REQUIRE([_GP_SLEEP_INIT])dnl
AS_IF([test -t 1], [dnl
[$]gp_sleep $1
])
])dnl
dnl
dnl
dnl ####################################################################
dnl
dnl Local Variables:
dnl mode: autoconf
dnl End:
