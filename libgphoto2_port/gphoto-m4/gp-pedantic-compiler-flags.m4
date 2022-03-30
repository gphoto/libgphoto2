# gp-pedantic-compiler-flags.m4 - have compiler warn a LOT     -*- Autoconf -*-
# serial 13
dnl | Increment the above serial number every time you edit this file.
dnl | When it finds multiple m4 files with the same name,
dnl | aclocal will use the one with the highest serial.
dnl
dnl ####################################################################
dnl GP_PEDANTIC_COMPILER_FLAGS & Co.
dnl ####################################################################
dnl
dnl
dnl ####################################################################
dnl GP_CONDITIONAL_COMPILE_FLAGS(FLAG_VAR, FLAGS)
dnl ####################################################################
AC_DEFUN([GP_CONDITIONAL_COMPILE_FLAGS], [dnl
dnl
# BEGIN $0($@)
dnl
m4_pushdef([the_lang],
           [m4_case([$1],
                    [CFLAGS], [C],
                    [CXXFLAGS], [C++],
                    [m4_fatal([unhandled compiler flags variable: ][$1])])])dnl
dnl
AC_LANG_PUSH(the_lang)dnl
dnl
dnl If $2 is a warning option, normalize it, as -Wno-foo will compile
dnl successfully, regardless of whether the compiler even knows the
dnl -Wfoo warning. FIXME: Split $2 and operate on each part?
m4_pushdef([normalized_option],
           [m4_bpatsubst([$2],
                         [^-W\(no-\)?\(error=\)?\([-_0-9a-zA-Z=]+\)$],
			 [-W\3])])dnl
dnl
saved_$1="${$1}"
$1="${saved_$1} normalized_option -Werror"
AC_MSG_CHECKING([whether $1 can append $2])
AC_COMPILE_IFELSE([dnl
m4_case(the_lang, [C], [dnl
AC_LANG_SOURCE([[
#include <stdio.h>
int main(int argc, char *argv[])
{
  int i;
  /* Use argc and argv to prevent warning about unused function params */
  for (i=0; i<argc; ++i) {
    printf("  %d %s\n", i, argv[i]);
  }
  return 0;
}
]])dnl
], [C++], [dnl
AC_LANG_SOURCE([[
#include <iostream>
int main(int argc, char *argv[])
{
  int i;
  /* Use argc and argv to prevent warning about unused function params */
  for (i=0; i<argc; ++i) {
    std::cout << "  " << i << " " << argv[i] << std::endl;
  }
  return 0;
}
]])dnl
], [m4_fatal([unhandled language ]the_lang)])dnl
], [dnl
  AC_MSG_RESULT([yes])
  $1="${saved_$1} $2"
], [dnl
  AC_MSG_RESULT([no])
  gp_have_pedantic_compiler=no
  $1="$saved_$1"
])
m4_popdef([normalized_option])dnl
AC_LANG_POP(the_lang)dnl
m4_popdef([the_lang])dnl
# END $0($@)
])dnl
dnl
dnl
dnl ####################################################################
dnl Params:
dnl   $1 Depending on the language, CFLAGS or CXXFLAGS
dnl   $2 The flag variable the caller wants us to set
dnl   $3 The AM_CONDITIONAL the caller wants us to set
dnl   $4 The -std= argument (such as -std=c++98 or -std=c99)
dnl ####################################################################
AC_DEFUN([__GP_PEDANTIC_COMPILER_FLAGS], [dnl
# BEGIN $0($@)
gp_have_pedantic_compiler=yes
GP_CONDITIONAL_COMPILE_FLAGS([$1], [$4])dnl
AS_VAR_IF([gp_have_pedantic_compiler], [yes], [dnl
  GP_CONDITIONAL_COMPILE_FLAGS([$1], [-pedantic -Wall -Wextra -Werror])dnl
])
AM_CONDITIONAL([$3], [test "x$gp_have_pedantic_compiler" = "xyes"])
AC_SUBST([$2], ["[$]$1"])
AC_MSG_CHECKING([whether to test pedantic ][$4])
AM_COND_IF([$3], [dnl
  AC_MSG_RESULT([yes])
], [dnl
  AC_MSG_RESULT([no])
])
# END $0($@)
])dnl
dnl
dnl
dnl ####################################################################
dnl Params:
dnl   $1 Depending on the language, CFLAGS or CXXFLAGS
dnl   $2 The variable/conditional component the caller wants us to use
dnl   $3 The -std= argument (such as -std=c++98 or -std=c99)
dnl ####################################################################
m4_pattern_allow([GP_PEDANTIC_CFLAGS_])dnl
m4_pattern_allow([GP_PEDANTIC_CXXFLAGS_])dnl
m4_pattern_allow([GP_HAVE_PEDANTIC_FLAGS_])dnl
m4_pattern_allow([GP_CONDITIONAL_COMPILE_FLAGS])dnl
AC_DEFUN([_GP_PEDANTIC_COMPILER_FLAGS], [dnl
# BEGIN $0($@)
gp_compiler_flags_saved_$1="[$]$1"
$1=""
__GP_PEDANTIC_COMPILER_FLAGS([$1],
                             [GP_PEDANTIC_][$1][_][$2],
                             [GP_HAVE_PEDANTIC_FLAGS_][$2],
                             [$3])
GP_PEDANTIC_$1_$2="[$]$1"
$1="[$]gp_compiler_flags_saved_$1"
# END $0($@)
])dnl
dnl
dnl
dnl ####################################################################
dnl GP_PEDANTIC_COMPILER_FLAGS(VAR_COMPONENT, LANG, std-arg)
dnl Start a new series of compiler flags
dnl ####################################################################
AC_DEFUN([GP_PEDANTIC_COMPILER_FLAGS], [dnl
# BEGIN $0($@)
AC_LANG_PUSH([$2])
_GP_PEDANTIC_COMPILER_FLAGS(m4_case($2,[C],[CFLAGS],[C++],[CXXFLAGS],[m4_fatal([Unknown language given: ][$2])]), [$1], [$3])
AC_LANG_POP([$2])
# END $0($@)
])dnl
dnl
dnl
dnl ####################################################################
dnl
dnl GP_WITH_EMPTY_CONFDEFS_H
dnl
dnl Usage:
dnl     GP_WITH_EMPTY_CONFDEFS_H([
dnl       GP_CONDITIONAL_COMPILE_FLAGS([CFLAGS], [-Wno-foo])
dnl       GP_CONDITIONAL_COMPILE_FLAGS([CFLAGS], [-Wno-bar])
dnl     ])dnl
dnl
dnl Since autoconf 2.63b, the macros defined by AC_DEFINE so far
dnl (mostly PACKAGE, VERSION, etc.) are in confdefs.h by now and are
dnl included using AC_LANG_SOURCE or AC_LANG_PROGRAM.
dnl
dnl We cannot use a confdefs.h with macro definitions for the pedantic
dnl compilation tests as that pedantic compilation might complain
dnl about macros being defined but not used.
dnl
dnl We also cannot just put AC_LANG_DEFINES_PROVIDED inside the
dnl program, as that will make sure the program starts with the
dnl current content of confdefs.h.
dnl
dnl In order to be able to test compile programs without any macro
dnl definitions from confdefs.ht, we save the original confdefs.h file
dnl and use an empty confdefs.h for our checks, then restore the
dnl original confdefs.h after our checks are done.
dnl
dnl Of course, we must not append to confdefs.h by e.g. calling
dnl AC_DEFINE or AC_DEFINE_UNQUOTED until the original confdefs.h has
dnl been restored, as those definitions would then be lost.
dnl
dnl ####################################################################
AC_DEFUN([GP_WITH_EMPTY_CONFDEFS_H], [dnl
m4_pushdef([AC_DEFINE],
           [m4_fatal([Must not use AC_DEFINE inside $0])])dnl
m4_pushdef([AC_DEFINE_UNQUOTED],
           [m4_fatal([Must not use AC_DEFINE_UNQUOTED inside $0])])dnl
cat confdefs.h
AC_MSG_NOTICE([saving original confdefs.h and creating empty confdefs.h])
rm -f confdefs.h.saved
mv confdefs.h confdefs.h.saved
cat <<EOF >confdefs.h
/* confdefs.h forced empty for the pedantic compile tests */
EOF
dnl
AC_MSG_CHECKING([CFLAGS value before])
AC_MSG_RESULT([$CFLAGS])
AC_MSG_CHECKING([CXXFLAGS value before])
AC_MSG_RESULT([$CXXFLAGS])
dnl
$1
dnl
AC_MSG_CHECKING([CFLAGS value after])
AC_MSG_RESULT([$CFLAGS])
AC_MSG_CHECKING([CXXFLAGS value after])
AC_MSG_RESULT([$CXXFLAGS])
dnl
AC_MSG_NOTICE([restoring original confdefs.h])
rm -f confdefs.h
mv confdefs.h.saved confdefs.h
cat confdefs.h
m4_popdef([AC_DEFINE_UNQUOTED])dnl
m4_popdef([AC_DEFINE])dnl
])dnl
dnl
dnl
dnl ####################################################################
dnl
dnl Local Variables:
dnl mode: autoconf
dnl End:
