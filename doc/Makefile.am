SUBDIRS = api figures

# distribute manual Portable Document Format (PDF) if possible
manual_pdf = @manual_pdf@

# distribute manual in PostScript (PS) format if possible
manual_ps = @manual_ps@

# build HTML manual if possible
manual_html = @manual_html@

EXTRA_DIST = DAEMON build_OS2.txt 					\
	camlib_devel.lyx 						\
	camlib_devel-portmon.eps camlib_devel-repeater.eps 		\
	camlib_devel.sgml camlib_devel.txt camlib_devel-y.eps 		\
	frontend_devel.lyx frontend_devel.sgml frontend_devel.txt 	\
	gphoto2-cli.txt gphoto2.txt					\
	gphoto2.xml $(man_MANS) $(manual_pdf) $(manual_ps)		\
	FAQ

man_MANS = gphoto2.1 gphoto2.3 gphoto2_port.3

docdir = @DOC_DIR@

doc_DATA = gphoto2-cli.txt gphoto2.txt FAQ


########################################################################
# The following rules are for regenerating man pages, pdf, html and ps
# docs from gphoto2.xml. This is only done when xmlto is found, which
# can be obtained from <URL:http://cyberelk.net/tim/xmlto/>.
#
# We should not forget the alternative dummy targets, as otherwise
# the build process will barf when xmlto is not present.
########################################################################


########################################################################
if XMLTOPDF
gphoto2.pdf: gphoto2.xml
	xmlto pdf $<
endif
########################################################################


########################################################################
if XMLTOPS
gphoto2.ps: gphoto2.xml
	xmlto ps $<
endif
########################################################################


########################################################################
if XMLTOMAN
$(man_MANS): gphoto2.xml
	xmlto man $<
endif
########################################################################


.PHONY: clean-local-manual
.PHONY: install-data-local-manual
.PHONY: dist-hook-manual


########################################################################
if XMLTOHTML

# do not empty this variable
htmldir=@HTML_DIR@

TARGET_DIR=$(htmldir)/manual

manual: gphoto2.xml
	xmlto html -o "$@" "$<"
	touch "$@"

clean_local_manual = clean-local-manual
clean-local-manual:
	rm -rf "manual/"

install_data_local_manual = install-data-local-manual
install-data-local-manual:
	$(INSTALL) -d -m 0755 $(DESTDIR)$(TARGET_DIR)
	$(INSTALL_DATA) ./manual/*.html $(DESTDIR)$(TARGET_DIR)
	$(INSTALL) -d -m 0755 $(DESTDIR)$(TARGET_DIR)/figures
	$(INSTALL_DATA) ./figures/*.png $(DESTDIR)$(TARGET_DIR)/figures

dist_hook_manual = dist-hook-manual
dist-hook-manual:
	(cd $(srcdir) ; tar cvf - manual/) | (cd $(distdir); tar xf -)
	(cd $(srcdir) ; tar cvf - $(figure_dir)) | (cd $(distdir)/manual/; tar xf -)
endif
########################################################################



########################################################################
clean-local: $(clean_local_manual)
	@echo "Dummy $@ target"

install-data-local: $(install_data_local_manual)
	@echo "Dummy $@ target"

dist-hook: $(dist_hook_manual)
	@echo "Dummy $@ target"

all-local: $(man_MANS) $(manual_pdf) $(manual_ps) $(manual_html)
	@echo "Dummy $@ target"
