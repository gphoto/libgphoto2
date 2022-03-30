# gp-documentation.m4 - whether to build our gtk-doc docs      -*- Autoconf -*-
# serial 13
dnl | Increment the above serial number every time you edit this file.
dnl | When it finds multiple m4 files with the same name,
dnl | aclocal will use the one with the highest serial.
dnl
dnl ####################################################################
dnl check whether to build docs and where to:
dnl
dnl * determine presence of prerequisites (only gtk-doc for now)
dnl * determine destination directory for HTML files
dnl ####################################################################
dnl
AC_DEFUN([GP_BUILD_GTK_DOCS], [dnl
# ---------------------------------------------------------------------------
# gtk-doc: We use gtk-doc for building our documentation. However, we
#          require the user to explicitly request the build.
# ---------------------------------------------------------------------------
try_gtkdoc=false
gtkdoc_msg="no (not requested)"
have_gtkdoc=false
AC_ARG_ENABLE([docs],
[AS_HELP_STRING([--enable-docs],
[Use gtk-doc to build documentation [default=no]])],[
	if test x$enableval = xyes; then
		try_gtkdoc=true
	fi
])
if $try_gtkdoc; then
	AC_PATH_PROG([GTKDOC],[gtkdoc-mkdb])
	if test -n "${GTKDOC}"; then
		have_gtkdoc=true
		gtkdoc_msg="yes"
	else
		gtkdoc_msg="no (http://www.gtk.org/rdp/download.html)"
	fi
fi
AM_CONDITIONAL([ENABLE_GTK_DOC], [$have_gtkdoc])
GP_CONFIG_MSG([build API docs with gtk-doc],[$gtkdoc_msg])

apidocdir="${htmldir}/api"
AC_SUBST([apidocdir])
])dnl
dnl
dnl
dnl ####################################################################
dnl
dnl Local Variables:
dnl mode: autoconf
dnl End:
