SUBDIRS = api figures

EXTRA_DIST = DAEMON build_OS2.txt 					\
	camlib_devel.lyx 						\
	camlib_devel-portmon.eps camlib_devel-repeater.eps 		\
	camlib_devel.sgml camlib_devel.txt camlib_devel-y.eps 		\
	frontend_devel.lyx frontend_devel.sgml frontend_devel.txt 	\
	gphoto2-cli.txt gphoto2.txt					\
	gphoto2.xml $(man_MANS) 					\
	FAQ

man_MANS = gphoto2.1 gphoto2.3 gphoto2_port.3

docdir = @DOC_DIR@

doc_DATA = gphoto2-cli.txt gphoto2.txt FAQ

# Special rule for regenerating the man page from the XML file.
# This requires xmlto, which can be obtained from
# <http://cyberelk.net/tim/xmlto/>.
if XMLTOMAN
all-local: $(man_MANS)

$(man_MANS): gphoto2.xml
	xmlto man $<
endif

if XMLTOHTML
htmldir=@HTML_DIR@

TARGET_DIR=$(htmldir)/manual

all-local: manual

manual: gphoto2.xml
	xmlto html -o $@ $<
	touch $@

install-data-local:
	$(INSTALL) -d -m 0755 $(DESTDIR)$(TARGET_DIR)
	$(INSTALL_DATA) ./manual/*.html $(DESTDIR)$(TARGET_DIR)

dist-hook:
	(cd $(srcdir) ; tar cvf - manual) | (cd $(distdir); tar xf -)
endif
