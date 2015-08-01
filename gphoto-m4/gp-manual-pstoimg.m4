dnl ---------------------------------------------------------------------------
dnl pstoimg: This program is needed for processing images. If not found,
dnl          documentation can still be built, but without figures.
dnl ---------------------------------------------------------------------------
AC_DEFUN([GP_CHECK_PSTOIMG],
[
AC_REQUIRE([GP_CHECK_DOT])dnl for creating tesseract.ps

try_pstoimg=true
have_pstoimg=false
AC_ARG_WITH(pstoimg, AS_HELP_STRING([--without-pstoimg], [Do not use pstoimg]),[
	if test "x$withval" = "xno"; then
		try_pstoimg=false
	fi])
if $try_pstoimg; then
	AC_PATH_PROG(PSTOIMG,pstoimg)
	if test -n "${PSTOIMG}"; then
		have_pstoimg=true
	fi
fi
if $have_pstoimg; then
	AC_SUBST(PSTOIMG)
	AC_MSG_CHECKING([whether ${PSTOIMG} works])
	rm -f tesseract.png
        ${PSTOIMG} -type png -scale 1.2 -antialias -crop a tesseract.ps > /dev/null
        if test $? != 0 || test ! -f tesseract.png; then
                have_pstoimg=false
		AC_MSG_RESULT(no)
	else
		AC_MSG_RESULT(yes)
        fi
	rm -f tesseract.png
fi
AM_CONDITIONAL(ENABLE_PSTOIMG, $have_pstoimg)

])
