SUBDIRS = api

#
# The intention of these macros, targets, rules, and variables is the
# following:
#
# A distribution tarball should have the manuals (HTML, PDF, PS) included.
# Only if the user has the software to re-build the manuals from the
# docbook source, should we delete or re-create the created docs.
#

EXTRA_DIST = DAEMON build_OS2.txt 					\
	camlib_devel.lyx 						\
	camlib_devel-portmon.eps camlib_devel-repeater.eps 		\
	camlib_devel.sgml camlib_devel.txt camlib_devel-y.eps 		\
	frontend_devel.lyx frontend_devel.sgml frontend_devel.txt 	\
	gphoto2-cli.txt							\
	FAQ faq.xml

man_MANS = gphoto2.3 gphoto2_port.3

docdir = @DOC_DIR@

doc_DATA = FAQ faq.xml
