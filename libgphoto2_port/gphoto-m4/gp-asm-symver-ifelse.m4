# gp-asm-symver-ifelse.m4 - check for asm .symver              -*- Autoconf -*-
# serial 13
dnl | Increment the above serial number every time you edit this file.
dnl | When it finds multiple m4 files with the same name,
dnl | aclocal will use the one with the highest serial.
dnl
dnl ####################################################################
dnl GP_ASM_SYMVER_IFELSE([ACTION-IF-TRUE], [ACTION-IF-FALSE])
dnl ####################################################################
dnl
AC_DEFUN([GP_ASM_SYMVER_IFELSE], [dnl
AC_MSG_CHECKING([for asm .symver support])
AC_COMPILE_IFELSE([dnl
  AC_LANG_PROGRAM([[
    void f1(void);
    void f1() {}
    void f2(void);
    void f2() {}
    asm(".symver f1, f@VER1");
    asm(".symver f2, f@@VER2");
  ]], [[
  ]])dnl
], [dnl
AC_MSG_RESULT([yes])
$1
], [dnl
AC_MSG_RESULT([no])
$2
])
])dnl
dnl
dnl ####################################################################
dnl
dnl Local Variables:
dnl mode: autoconf
dnl End:
