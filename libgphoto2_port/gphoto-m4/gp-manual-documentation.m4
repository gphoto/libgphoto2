dnl
dnl check where to install documentation
dnl
dnl determines documentation "root directory", i.e. the directory
dnl where all documentation will be placed in
dnl

AC_DEFUN([GP_CHECK_DOCDIR],
[

AC_ARG_WITH(doc-dir, [  --with-doc-dir=PATH       Where to install docs  [default=autodetect]])dnl

# check for the main ("root") documentation directory
AC_MSG_CHECKING([main docdir])

if test "x${with_doc_dir}" != "x"
then # docdir is given as parameter
    docdir="${with_doc_dir}"
    AC_MSG_RESULT([${docdir} (from parameter)])
else # otherwise invent a docdir hopefully compatible with system policy
    if test -d "/usr/share/doc"
    then
        maindocdir='${prefix}/share/doc'
        AC_MSG_RESULT([${maindocdir} (FHS style)])
    elif test -d "/usr/doc"
    then
        maindocdir='${prefix}/doc'
        AC_MSG_RESULT([${maindocdir} (old style)])
    else
        maindocdir='${datadir}/doc'
        AC_MSG_RESULT([${maindocdir} (default value)])
    fi
    AC_MSG_CHECKING(package docdir)
    # check whether to include package version into documentation path
    # FIXME: doesn't work properly.
    if ls -d /usr/{share/,}doc/*-[[]0-9[]]* > /dev/null 2>&1
    then
        docdir="${maindocdir}/${PACKAGE}-${VERSION}"
        AC_MSG_RESULT([${docdir} (redhat style)])
    else
        docdir="${maindocdir}/${PACKAGE}"
        AC_MSG_RESULT([${docdir} (default style)])
    fi
fi

AC_SUBST(docdir)

])dnl

dnl Solaris hack for grep and tr
AC_DEFUN([GP_CHECK_TR],
[
if test -n "`echo $host_os | grep '[sS]olaris'`"; then
  TR=/usr/xpg4/bin/tr
  GREP=/usr/xpg4/bin/grep  
else
  TR=tr
  GREP=grep
fi	
])

dnl
dnl check whether to build docs and where to:
dnl
dnl * determine presence of prerequisites (only gtk-doc for now)
dnl * determine destination directory for HTML files
dnl

AC_DEFUN([GP_BUILD_DOCS],
[
# doc dir has to be determined in advance
AC_REQUIRE([GP_CHECK_DOCDIR])
AC_REQUIRE([GP_CHECK_GTK_DOC])
AC_REQUIRE([GP_CHECK_FIG2DEV])
AC_REQUIRE([GP_CHECK_DOCBOOK_XML])
AC_REQUIRE([GP_CHECK_TR])
AC_REQUIRE([GP_CHECK_PSTOIMG])
AC_REQUIRE([GP_CHECK_DOT])
AC_REQUIRE([GP_CHECK_W3M])

gphoto2xml='$(top_srcdir)/src/gphoto2.xml'
AC_SUBST(gphoto2xml)

dnl ---------------------------------------------------------------------------
dnl Give the user the possibility to install documentation in
dnl user-defined locations.
dnl ---------------------------------------------------------------------------
AC_ARG_WITH(html-dir, [  --with-html-dir=PATH      Where to install html docs [default=autodetect]])
AC_MSG_CHECKING([for html dir])
if test "x${with_html_dir}" = "x" ; then
    htmldir="${docdir}/html"
    AC_MSG_RESULT([${htmldir} (default)])
else
    htmldir="${with_html_dir}"
    AC_MSG_RESULT([${htmldir} (from parameter)])
fi
AC_SUBST(htmldir)

AC_ARG_WITH(xhtml-dir, [  --with-xhtml-dir=PATH     Where to install xhtml docs [default=autodetect]])
AC_MSG_CHECKING([for xhtml dir])
if test "x${with_xhtml_dir}" = "x" ; then
    xhtmldir="${docdir}/xhtml"
    AC_MSG_RESULT([${xhtmldir} (default)])
else
    xhtmldir="${with_xhtml_dir}"
    AC_MSG_RESULT([${xhtmldir} (from parameter)])
fi
AC_SUBST(xhtmldir)

AC_ARG_WITH(html-nochunks-dir, [  --with-html-nochunks-dir=PATH      Where to install html-nochunks docs [default=autodetect]])
AC_MSG_CHECKING([for html-nochunks dir])
if test "x${with_html_nochunks_dir}" = "x" ; then
    htmlnochunksdir="${docdir}/html-nochunks"
    AC_MSG_RESULT([${htmlnochunksdir} (default)])
else
    htmlnochunksdir="${with_html_nochunks_dir}"
    AC_MSG_RESULT([${htmlnochunksdir} (from parameter)])
fi
AC_SUBST(htmlnochunksdir)

AC_ARG_WITH(xhtml-nochunks-dir, [  --with-xhtml-nochunks-dir=PATH      Where to install xhtml-nochunks docs [default=autodetect]])
AC_MSG_CHECKING([for xhtml-nochunks dir])
if test "x${with_xhtml_nochunks_dir}" = "x" ; then
    xhtmlnochunksdir="${docdir}/xhtml-nochunks"
    AC_MSG_RESULT([${xhtmlnochunksdir} (default)])
else
    xhtmlnochunksdir="${with_xhtml_nochunks_dir}"
    AC_MSG_RESULT([${xhtmlnochunksdir} (from parameter)])
fi
AC_SUBST(xhtmlnochunksdir)

AC_ARG_WITH(xml-dir, [  --with-xml-dir=PATH      Where to install xml docs [default=autodetect]])
AC_MSG_CHECKING([for xml dir])
if test "x${with_xml_dir}" = "x" ; then
    xmldir="${docdir}/xml"
    AC_MSG_RESULT([${xmldir} (default)])
else
    xmldir="${with_xml_dir}"
    AC_MSG_RESULT([${xmldir} (from parameter)])
fi
AC_SUBST(xmldir)
xmlcssdir="${xmldir}/css"
AC_SUBST(xmlcssdir)

AC_ARG_WITH(txt-dir, [  --with-txt-dir=PATH      Where to install txt docs [default=autodetect]])
AC_MSG_CHECKING([for txt dir])
if test "x${with_txt_dir}" = "x" ; then
    txtdir="${docdir}/txt"
    AC_MSG_RESULT([${txtdir} (default)])
else
    txtdir="${with_txt_dir}"
    AC_MSG_RESULT([${txtdir} (from parameter)])
fi
AC_SUBST(txtdir)

AC_ARG_WITH(man-dir, [  --with-man-dir=PATH      Where to install man docs [default=autodetect]])
AC_MSG_CHECKING([for man dir])
if test "x${with_man_dir}" = "x" ; then
    manmandir="${docdir}/man"
    AC_MSG_RESULT([${manmandir} (default)])
else
    manmandir="${with_man_dir}"
    AC_MSG_RESULT([${manmandir} (from parameter)])
fi
AC_SUBST(manmandir)

AC_ARG_WITH(pdf-dir, [  --with-pdf-dir=PATH      Where to install pdf docs [default=autodetect]])
AC_MSG_CHECKING([for pdf dir])
if test "x${with_pdf_dir}" = "x" ; then
    pdfdir="${docdir}/pdf"
    AC_MSG_RESULT([${pdfdir} (default)])
else
    pdfdir="${with_pdf_dir}"
    AC_MSG_RESULT([${pdfdir} (from parameter)])
fi
AC_SUBST(pdfdir)

AC_ARG_WITH(ps-dir, [  --with-ps-dir=PATH      Where to install ps docs [default=autodetect]])
AC_MSG_CHECKING([for ps dir])
if test "x${with_ps_dir}" = "x" ; then
    psdir="${docdir}/ps"
    AC_MSG_RESULT([${psdir} (default)])
else
    psdir="${with_ps_dir}"
    AC_MSG_RESULT([${psdir} (from parameter)])
fi
AC_SUBST(psdir)

AC_ARG_WITH(figure-dir, [  --with-figure-dir=PATH      Where to install figures [default=autodetect]])
AC_MSG_CHECKING([for figure dir])
if test "x${with_figure_dir}" = "x" ; then
    figuredir="${docdir}/figures"
    AC_MSG_RESULT([${figuredir} (default)])
else
    figuredir="${with_figure_dir}"
    AC_MSG_RESULT([${figuredir} (from parameter)])
fi
AC_SUBST(figuredir)

AC_ARG_WITH(screenshots-dir, [  --with-screenshots-dir=PATH      Where to install screenshotss [default=autodetect]])
AC_MSG_CHECKING([for screenshots dir])
if test "x${with_screenshots_dir}" = "x" ; then
    screenshotsdir="${docdir}/screenshots"
    AC_MSG_RESULT([${screenshotsdir} (default)])
else
    screenshotsdir="${with_screenshots_dir}"
    AC_MSG_RESULT([${screenshotsdir} (from parameter)])
fi
AC_SUBST(screenshotsdir)
screenshotsgtkamdir="${screenshotsdir}/gtkam"
AC_SUBST(screenshotsgtkamdir)

doc_formats_list='man html txt ps pdf'

# initialize have_xmlto* to false
for i in $doc_formats_list; do
  d=`echo $i | $TR A-Z a-z`
  eval "have_xmlto$d=false"
done

AC_MSG_CHECKING(checking doc formats)
AC_ARG_WITH(doc_formats,
  [  --with-doc-formats=<list>   create doc with format in <list>; ]
  [                            'all' build all doc formats; ]
  [                            possible formats are: ]
  [                            man, html, ps, pdf ],
  doc_formats="$withval", doc_formats="man html txt")

if test "$doc_formats" = "all"; then
  doc_formats=$doc_formats_list
else
  doc_formats=`echo $doc_formats | sed 's/,/ /g'`
fi

# set have_xmlto* to true if requested and possible
if $have_xmlto; then
  for i in $doc_formats; do
    if test -n "`echo $doc_formats_list | $GREP -E \"(^| )$i( |\$)\"`"; then
      eval "have_xmlto$i=true"
    else
      AC_ERROR(Unknown doc format $i!)
    fi
  done
  AC_MSG_RESULT($doc_formats)
else
  AC_MSG_RESULT([deactivated (requires xmlto)])
fi

# Make sure that xmltopdf actually works
if $have_xmltopdf; then
	AC_MSG_CHECKING([whether pdf creation works])
	oldcwd="$(pwd)"
	top_srcdir() { echo "$srcdir"; }
	mkdir test-pdf
	cd test-pdf
	cat>test-db.xml<<EOF
<!DOCTYPE book PUBLIC "-//OASIS//DTD DocBook XML V4.2//EN"
 "http://www.oasis-open.org/docbook/xml/4.2/docbookx.dtd">
<book id="foobook">
	<bookinfo>
		<title>Foo Title</title>
	</bookinfo>
	<chapter id="foochapter">
		<title>Foo Chapter</title>
		<para>
			Foo bar blah blah
		      <!--ulink url="https://www.hypothetical-url.invalid/with&amp;ersand">ampersand</ulink-->.
		</para>
	</chapter>
</book>
EOF
	${PURE_XMLTO} pdf -o . test-db.xml --searchpath ..
	if test -s test-db.pdf; then
		AC_MSG_RESULT([yes, look at $(pwd)/test-db.pdf to verify])
	else
		if $have_xmltopdf; then
			AC_MSG_ERROR([PDF creation requested, but failed. See $(pwd) ...])
		else
			AC_MSG_RESULT([no, but not requested])
		fi
	fi
	cd "$oldcwd"
	unset top_srcdir
fi

AM_CONDITIONAL(XMLTOHTML,$have_xmltohtml)
AM_CONDITIONAL(XMLTOMAN,$have_xmltoman)
AM_CONDITIONAL(XMLTOTXT,$have_xmltotxt)
AM_CONDITIONAL(XMLTOTXT2,$have_xmltotxt && $have_w3m)
AM_CONDITIONAL(XMLTOPDF,$have_xmltopdf)
AM_CONDITIONAL(XMLTOPS,$have_xmltops)

# create list of supported formats
AC_MSG_CHECKING([for manual formats to re­create])
xxx=""
manual_html=""
manual_pdf=""
manual_ps=""
if $have_xmltohtml; then
        xxx="${xxx} html"
fi
if $have_xmltoman; then
        xxx="${xxx} man"
fi
if $have_xmltopdf; then
        xxx="${xxx} pdf"
fi
if $have_xmltops; then 
        xxx="${xxx} ps"
fi
if $have_xmltotxt; then 
        xxx="${xxx} txt"
fi
AC_SUBST(manual_html)
AC_SUBST(manual_pdf)
AC_SUBST(manual_ps)
AC_MSG_RESULT($xxx)

if test "x$xxx" != "x"
then
        if $have_fig2dev; then
                fig_out=""
        else
                fig_out="out"
        fi
        manual_msg="in (${xxx} ) formats with${fig_out} figures"
fi

])dnl
