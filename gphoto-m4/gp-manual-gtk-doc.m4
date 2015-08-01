dnl ---------------------------------------------------------------------------
dnl gtk-doc: We use gtk-doc for building our documentation. However, we
dnl          require the user to explicitely request the build.
dnl ---------------------------------------------------------------------------
AC_DEFUN([GP_CHECK_GTK_DOC],
[
try_gtkdoc=false
gtkdoc_msg="no (not requested)"
have_gtkdoc=false
AC_ARG_ENABLE(docs, [  --enable-docs             Use gtk-doc to build documentation [default=no]],[
	if test x$enableval = xyes; then
		try_gtkdoc=true
	fi])
if $try_gtkdoc; then
	AC_PATH_PROG(GTKDOC,gtkdoc-mkdb)
	if test -n "${GTKDOC}"; then
		have_gtkdoc=true
		gtkdoc_msg="yes"
	else
		gtkdoc_msg="no (http://www.gtk.org/rdp/download.html)"
	fi
fi
AM_CONDITIONAL(ENABLE_GTK_DOC, $have_gtkdoc)
])
