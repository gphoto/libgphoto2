dnl ---------------------------------------------------------------------------
dnl w3m: This program is needed for converting HTML to text.
dnl ---------------------------------------------------------------------------
AC_DEFUN([GP_CHECK_W3M],
[

try_w3m=true
have_w3m=false
AC_ARG_WITH(w3m, [  --without-w3m         Don't use w3m],[
	if test "x$withval" = "xno"; then
		try_w3m=false
	fi])
if $try_w3m; then
	AC_PATH_PROG(W3M,w3m)
	if test -n "${W3M}"; then
		have_w3m=true
	fi
fi
if $have_w3m; then
	AC_MSG_CHECKING([whether ${W3M} works])
        cat > html2text-test.xxxhtml 2> /dev/null <<EOF
<html>
<title>
<h1>HTML2TEXT Test</h1>
</title>
<body>
<p>This is a test.</p>
</body>
</html>
EOF
	${W3M} -T text/html -cols 78 -dump html2text-test.xxxhtml > html2text-test.txt
        if test $? != 0 || test ! -f html2text-test.txt || test ! -s html2text-test.txt
        then
                have_w3m=false
		AC_MSG_RESULT([no (see http://www.w3m.org/ or http://w3m.sourceforge.net/ ...)])
		W3M=false
		AC_SUBST(W3M)
	else
		AC_MSG_RESULT([yes])
		AC_SUBST(W3M)
        fi
fi
AM_CONDITIONAL([HAVE_W3M], [$have_w3m])

])
