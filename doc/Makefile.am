SUBDIRS = api figures

#
# The intention of these macros, targets, rules, and variables is the
# following:
#
# A distribution tarball should have the manuals (HTML, PDF, PS) included.
# Only if the user has the software to re-build the manuals from the
# docbook source, should we delete or re-create the created docs.
#

# distribute manual Portable Document Format (PDF) if possible
manual_pdf = @manual_pdf@

# distribute manual in PostScript (PS) format if possible
manual_ps = @manual_ps@

# build HTML manual if possible
manual_html = @manual_html@

CLEAN_FILES = $(manual_pdf) $(manual_ps) figures-stamp

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

doc_DATA = gphoto2-cli.txt gphoto2.txt FAQ $(manual_pdf) $(manual_ps)

figures-stamp:
	$(MAKE) -C figures
	touch $@

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
gphoto2.pdf: gphoto2.xml figures-stamp
	test -e $$(basename $<) || ln -s $< # symlink file from srcdir to builddir if required
	$(XMLTO) pdf $$(basename $<) # work on local file or symlink
endif
########################################################################


########################################################################
if XMLTOPS
# doesn't work if builddir != srcdir
gphoto2.ps: gphoto2.xml figures-stamp
	test -e $$(basename $<) || ln -s $< # symlink file from srcdir to builddir if required
	$(XMLTO) ps $$(basename $<) # work on local file or symlink
endif
########################################################################


########################################################################
if XMLTOMAN
# works if builddir != srcdir
$(man_MANS): gphoto2.xml
	$(XMLTO) man $<
endif
########################################################################


.PHONY: clean-local-manual
.PHONY: install-data-local-manual
.PHONY: dist-hook-manual


########################################################################
if XMLTOHTML
# doesn't work correctly if builddir != srcdir

# do not empty this variable
htmldir=@HTML_DIR@

TARGET_DIR=$(htmldir)/manual

manual: gphoto2.xml
	$(XMLTO) html -o "$@" "$<"
	cd $@ && rm -f figures && ln -s ../figures figures # to view docs in src tree
	touch "$@"

clean_local_manual = clean-local-manual
clean-local-manual:
	rm -rf "manual/"

install_data_local_manual = install-data-local-manual
install-data-local-manual: manual figures-stamp
	$(INSTALL) -d -m 0755 $(DESTDIR)$(TARGET_DIR)
	$(INSTALL_DATA) ./manual/*.html $(DESTDIR)$(TARGET_DIR)
	$(INSTALL) -d -m 0755 $(DESTDIR)$(TARGET_DIR)/figures
	$(INSTALL_DATA) ./figures/*.png $(DESTDIR)$(TARGET_DIR)/figures

# copy HTML manual to distribution
dist_hook_manual = dist-hook-manual
dist-hook-manual: manual figures-stamp
	$(AMTAR) cf - manual/*.html | (cd $(distdir); $(AMTAR) xvf -)
if ENABLE_FIGURES
	$(AMTAR) cf - figures/*.png | (cd $(distdir)/manual/; $(AMTAR) xvf -)
endif
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
