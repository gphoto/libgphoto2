dnl
dnl check where to install documentation
dnl
dnl determines documentation "root directory", i.e. the directory
dnl where all documentation will be placed in
dnl

AC_DEFUN(GP_CHECK_DOC_DIR,
[
AC_ARG_WITH(doc-dir, [  --with-doc-dir=PATH  where to install docs ])

# check for the main ("root") documentation directory
AC_MSG_CHECKING([main docdir])
if test "x${with_doc_dir}" != "x"
then # docdir given as parameter
    DOC_DIR="${with_doc_dir}"
    AC_MSG_RESULT([${DOC_DIR} (from parameter)])
else
    if test -d "/usr/share/doc"
    then
        maindocdir='${prefix}/share/doc'
        AC_MSG_RESULT([${maindocdir}])
    elif test -d "/usr/doc"
    then
        maindocdir='${prefix}/doc'
        AC_MSG_RESULT([${maindocdir}])
    else
        maindocdir='${datadir}/doc'
        AC_MSG_RESULT([${maindocdir} (default value)])
    fi
    AC_MSG_CHECKING(package docdir)
    # check whether to include package version into documentation path
    if ls -d /usr/{share/,}doc/*-[[]0-9[]]* > /dev/null 2>&1
    then
        DOC_DIR="${maindocdir}/${PACKAGE}-${VERSION}"
        AC_MSG_RESULT([${DOC_DIR} (redhat style)])
    else
        DOC_DIR="${maindocdir}/${PACKAGE}"
        AC_MSG_RESULT([${DOC_DIR} (default style)])
    fi
fi

AC_SUBST(DOC_DIR)

])dnl

dnl
dnl check whether to build docs and where to:
dnl
dnl * determine presence of prerequisites (only gtk-doc for now)
dnl * determine destination directory for HTML files
dnl

AC_DEFUN(GP_BUILD_DOCS,
[
# doc dir has to be determined in advance
AC_REQUIRE([GP_CHECK_DOC_DIR])

# check for the gtk-doc documentation generation utility
AC_CHECK_PROG(GTKDOC, gtkdoc-mkdb, true, false)
AM_CONDITIONAL(HAVE_GTK_DOC, $GTKDOC)
AC_SUBST(HAVE_GTK_DOC)

# check for the transfig graphics file conversion utility
AC_CHECK_PROG(TRANSFIG, gtkdoc-mkdb, true, false)
AM_CONDITIONAL(HAVE_TRANSFIG, $TRANSFIG)
AC_SUBST(HAVE_TRANSFIG)

# check whether we are to build docs
AC_ARG_ENABLE(docs, [  --enable-docs  Use gtk-doc to build documentation [default=no]], enable_docs="$enableval", enable_docs=no)

# check whether we can and will build docs
if test "$enable_docs" = "yes" ; then
    if test "$GTKDOC" = "true" ; then
        enable_docs=yes
    else
        enable_docs=no
	# if we can't fulfill an explicit wish, we don't just
	# warn them but complain and stop
        AC_MSG_ERROR([
You told us explicitly to build the docs with gtk-doc, but I
can't find the gtk-doc program. Aborting further configuration.
])
    fi
    if test "$TRANSFIG" != "true"; then
        AC_MSG_WARN([
You told us explicitly to build the docs, but I can't find the
transfig program. Therefore you won't have illustrations in 
your documentation.
])
    fi
fi
AM_CONDITIONAL(ENABLE_FIGURES, test "x$TRANSFIG" = "xtrue")
AM_CONDITIONAL(ENABLE_GTK_DOC, test "x$enable_docs" = "xyes")

# check where to install HTML docs
AC_ARG_WITH(html-dir, [  --with-html-dir=PATH  where to install html docs ])
AC_MSG_CHECKING([html dir])
if test "x${with_html_dir}" = "x" ; then
    HTML_DIR="${DOC_DIR}/html"
    AC_MSG_RESULT([${HTML_DIR} (default)])
else
    HTML_DIR="${with_html_dir}"
    AC_MSG_RESULT([${HTML_DIR} (from parameter)])
fi
AC_SUBST(HTML_DIR)

])dnl
