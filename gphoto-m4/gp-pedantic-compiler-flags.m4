dnl ####################################################################
dnl GP_PEDANTIC_COMPILER_FLAGS & Co.
dnl ####################################################################
dnl
dnl
dnl ####################################################################
dnl _GP_CONDITIONAL_COMPILE_FLAGS(FLAG_VAR, FLAGS)
dnl ####################################################################
AC_DEFUN([_GP_CONDITIONAL_COMPILE_FLAGS], [dnl
# BEGIN $0($@)
AS_VAR_IF([$1], [], [dnl
  $1="$2"
], [dnl
  $1="[$]{$1} $2"
])
AC_MSG_CHECKING([whether $1="[$]$1" compiles])
AC_COMPILE_IFELSE([m4_case([$1], [CFLAGS], [dnl
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
], [CXXFLAGS], [dnl
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
], [m4_fatal([wrong compiler flag])])], [dnl
  : "Added flag $2 to $1 and it works"
  AC_MSG_RESULT([yes])
], [dnl
  : "Added flag $2 to $1 and it does not work"
  AC_MSG_RESULT([no])
  gp_have_pedantic_compiler=no
])
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
_GP_CONDITIONAL_COMPILE_FLAGS([$1], [$4])dnl
AS_VAR_IF([gp_have_pedantic_compiler], [yes], [dnl
  _GP_CONDITIONAL_COMPILE_FLAGS([$1], [-pedantic -Wall -Wextra -Werror])dnl
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
dnl Local Variables:
dnl mode: autoconf
dnl End:
