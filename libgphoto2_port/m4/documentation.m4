dnl
dnl check where to install documentation
dnl

AC_DEFUN(GP_CHECK_DOC_DIR,
[
AC_ARG_WITH(doc-dir, [  --with-doc-dir=PATH  where to install docs ])

# check for the main ("root") documentation directory
if test "x${with_doc_dir}" = "x"
then
    echo -n "checking for main (root) docdir... "
    if test -d "/usr/share/doc"
    then
        maindocdir='${prefix}/share/doc'
        echo "${maindocdir}"
    elif test -d "/usr/doc"
    then
        maindocdir='${prefix}/doc'
        echo "${maindocdir}"
    else
        maindocdir='${datadir}/doc'
        echo "${maindocdir} (default value)"
    fi
    echo -n "checking for docdir... "
    # check whether to include package version into documentation path
    if ls -d /usr/{share/,}doc/*-[[]0-9[]]* > /dev/null 2>&1
    then
        DOC_DIR="${maindocdir}/${PACKAGE}-${VERSION}"
        echo "${DOC_DIR} (redhat style)"
    else
        DOC_DIR="${maindocdir}/${PACKAGE}"
        echo "${DOC_DIR} (default style)"
    fi
else
    DOC_DIR="${with_doc_dir}"
    echo "${DOC_DIR} (from parameter)"
fi
AC_SUBST(DOC_DIR)
])dnl

dnl
dnl check for gtk-doc and html directory
dnl

AC_DEFUN(GP_CHECK_GTK_DOC,
[
AC_REQUIRE([GP_CHECK_DOC_DIR])
AC_CHECK_PROG(GTKDOC, gtkdoc-mkdb, true, false)
AM_CONDITIONAL(HAVE_GTK_DOC, $GTKDOC)
AC_SUBST(HAVE_GTK_DOC)
AC_ARG_ENABLE(gtk-doc, [  --enable-gtk-doc  Use gtk-doc to build documentation [default=auto]], enable_gtk_doc="$enableval", enable_gtk_doc=auto)
if test x$enable_gtk_doc = xauto ; then
    if test x$GTKDOC = xtrue ; then
        enable_gtk_doc=yes
    else
        enable_gtk_doc=no
    fi
fi
AM_CONDITIONAL(ENABLE_GTK_DOC, test x$enable_gtk_doc = xyes)
AC_ARG_WITH(html-dir, [  --with-html-dir=PATH  where to install html docs ])

echo -n "checking for html dir... "
if test "x${with_html_dir}" = "x" ; then
    HTML_DIR="${DOC_DIR}/html"
    echo "${HTML_DIR} (default)"
else
    HTML_DIR="${with_html_dir}"
    echo "${HTML_DIR} (from parameter)"
fi
AC_SUBST(HTML_DIR)

])dnl
