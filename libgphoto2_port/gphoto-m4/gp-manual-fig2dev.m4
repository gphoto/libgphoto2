dnl ---------------------------------------------------------------------------
dnl fig2dev: This program is needed for processing images. If not found,
dnl          documentation can still be built, but without figures.
dnl ---------------------------------------------------------------------------
AC_DEFUN([GP_CHECK_FIG2DEV],
[

try_fig2dev=true
have_fig2dev=false
AC_ARG_WITH(fig2dev, [  --without-fig2dev         Don't use fig2dev],[
	if test "x$withval" = "xno"; then
		try_fig2dev=false
	fi])
if $try_fig2dev; then
	AC_PATH_PROG(FIG2DEV,fig2dev)
	if test -n "${FIG2DEV}"; then
		have_fig2dev=true
	fi
fi
if $have_fig2dev; then
	AC_SUBST(FIG2DEV)
        ${FIG2DEV} -L ps > /dev/null <<EOF
#FIG 3.2
Landscape
Center
Inches
Letter  
100.00
Single
-2
1200 2
1 3 0 1 0 7 50 0 -1 0.000 1 0.0000 3000 3750 270 270 3000 3750 3150 3975
EOF
        if test $? != 0; then
                have_fig2dev=false
        fi
fi
AM_CONDITIONAL(ENABLE_FIGURES, $have_fig2dev)

])
