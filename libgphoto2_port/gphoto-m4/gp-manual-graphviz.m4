# gp-manual-graphviz.m4 - look for graphviz dot tool; check it -*- Autoconf -*-
# serial 13
dnl | Increment the above serial number every time you edit this file.
dnl | When it finds multiple m4 files with the same name,
dnl | aclocal will use the one with the highest serial.
dnl
dnl ---------------------------------------------------------------------------
dnl dot: This program is needed for processing images. If not found,
dnl          documentation can still be built, but without figures.
dnl ---------------------------------------------------------------------------
AC_DEFUN([GP_CHECK_DOT],
[

try_dot=true
have_dot=false
AC_ARG_WITH(dot, AS_HELP_STRING([--without-dot], [Do not use dot]), [
	if test "x$withval" = "xno"; then
		try_dot=false
	fi])
if $try_dot; then
	AC_PATH_PROG(DOT,dot)
	if test -n "${DOT}"; then
		have_dot=true
	fi
fi
if $have_dot; then
	AC_SUBST(DOT)
	AC_MSG_CHECKING([whether ${DOT} works])
        ${DOT} -Tps -o tesseract.ps 2> /dev/null <<EOF
graph tesseract {
	node [[shape=point]];
	o -- {a;b;c;d;}
	a -- {ab;ac;ad;}
	b -- {ab;bc;bd;}
	c -- {ac;bc;cd;}
	d -- {ad;bd;cd;}
	ab -- {abc;abd;}
	ac -- {abc;acd;}
	ad -- {abd;acd;}
	bc -- {abc;bcd;}
	bd -- {abd;bcd;}
	cd -- {acd;bcd;}
	{abc;abd;acd;bcd;} -- abcd;
}
EOF
        if test $? != 0 || test ! -f tesseract.ps; then
                have_dot=false
		AC_MSG_RESULT([no (see http://www.graphviz.org/ ...)])
	else
		AC_MSG_RESULT(yes)
        fi
fi
AM_CONDITIONAL(ENABLE_GRAPHS, $have_dot)

])
dnl
dnl
dnl ####################################################################
dnl
dnl Local Variables:
dnl mode: autoconf
dnl End:
