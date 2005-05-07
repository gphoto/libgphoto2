dnl ------------------------------------------------------------------------
dnl try to find xmlto (required for generation of man pages and html docs)
dnl ------------------------------------------------------------------------
AC_DEFUN([GP_CHECK_DOCBOOK_XML],
[

AC_MSG_CHECKING([for XML catalogs])
XML_CATALOG_FILES="`find /etc/xml /usr/share/xml /usr/share/sgml -type f \( -iname 'catalog.xml' -or -iname 'catalog' \) -print 2> /dev/null | while read file; do echo -n "$file "; done`"
if test "x$XML_CATALOG_FILES" = "x"
then
	AC_MSG_RESULT([none found.])
else
	AC_MSG_RESULT([found ${XML_CATALOG_FILES}])
fi
AC_SUBST(XML_CATALOG_FILES)

#XML_DEBUG_CATALOG=0
#AC_SUBST(XML_DEBUG_CATALOG)

manual_msg="no (http://cyberelk.net/tim/xmlto/)"
try_xmlto=true
have_xmlto=false
AC_ARG_WITH(xmlto, [  --without-xmlto           Don't use xmlto],[
	if test x$withval = xno; then
		try_xmlto=false
	fi])
if $try_xmlto; then
	AC_PATH_PROG(XMLTO,xmlto)
	if test -n "${XMLTO}"; then
		have_xmlto=true
		manual_msg="yes"
		PURE_XMLTO="$XMLTO"
		if true || test "x$XML_CATALOG_FILES" = "x"; then
			unset XML_CATALOG_FILES
			XMLTO="${XMLTO} -m \$(top_srcdir)/src/xsl/custom.xsl"
		else
			XMLTO="env XML_CATALOG_FILES=\"${XML_CATALOG_FILES}\" ${XMLTO} -m ${top_srcdir}/src/xsl/custom.xsl"
		fi
        else
                # in case anybody runs $(XMLTO) somewhere, we return false
                XMLTO=false
	fi
fi

AM_CONDITIONAL(XMLTO, $have_xmlto)
])
