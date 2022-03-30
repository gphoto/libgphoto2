# gp-asm-symver-support.m4 - check for asm .symver; def macros -*- Autoconf -*-
# serial 13
dnl | Increment the above serial number every time you edit this file.
dnl | When it finds multiple m4 files with the same name,
dnl | aclocal will use the one with the highest serial.
dnl
dnl ####################################################################
dnl If compiler supports asm .symver, define HAVE_ASM_SYMVER C preprocessor macro and Automake conditional
dnl ####################################################################
dnl
AC_DEFUN([GP_ASM_SYMVER_SUPPORT], [dnl
GP_ASM_SYMVER_IFELSE([dnl
  have_asm_symver=yes
  AC_DEFINE([HAVE_ASM_SYMVERS], [1],
            [Define if there is asm .symver support.])
], [dnl
  have_asm_symver=no
])
AM_CONDITIONAL([HAVE_ASM_SYMVER],
               [test "x$have_asm_symver" = xyes])
])dnl
dnl
dnl ####################################################################
dnl
dnl Local Variables:
dnl mode: autoconf
dnl End:
