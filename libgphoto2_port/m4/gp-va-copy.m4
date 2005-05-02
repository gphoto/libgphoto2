dnl @synopsis GP_VA_COPY
dnl
dnl Checks whether one of these compiles and links:
dnl 1. va_copy()
dnl 2. __va_copy()
dnl 3. fallback
dnl
dnl In case of 1 or 2, AC_DEFINE(HAVE_VA_COPY).
dnl In case of 2, AC_DEFINE(va_copy,__va_copy)
dnl
dnl In code, use it like this
dnl #ifdef HAVE_VA_COPY
dnl    ... code with va_copy ...
dnl #else
dnl    ... code without va_copy or with error ...
dnl #endif
dnl
AC_DEFUN([GP_VA_COPY],[dnl
dnl
AC_CHECK_HEADER([stdarg.h],[],[
	AC_MSG_ERROR([
Building $PACKAGE_NAME requires <stdarg.h>.
])
])
dnl
have_va_copy=no
AC_TRY_LINK([
	#include <stdarg.h>
],[
	va_list a,b;
	va_copy(a,b);
],[
	have_va_copy="va_copy"
],[
	AC_TRY_LINK([
		#include <stdarg.h>
	],[
		va_list a,b;
		__va_copy(a,b);
	],[
		have_va_copy="__va_copy"
		AC_DEFINE([va_copy],[__va_copy],[__va_copy() was the originally proposed name])
	])
])
dnl
AC_MSG_CHECKING([for va_copy() or replacement])
AC_MSG_RESULT([$have_va_copy])
dnl
if test "x$have_va_copy" != "xno"; then
	AC_DEFINE([HAVE_VA_COPY],1,[Whether we have the va_copy() function])
fi
])dnl
dnl
dnl
dnl Local Variables:
dnl mode: autoconf
dnl End:
