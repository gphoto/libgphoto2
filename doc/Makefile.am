SUBDIRS = api

old_EXTRA_DIST = DAEMON build_OS2.txt 					\
	camlib_devel.lyx 						\
	camlib_devel-portmon.eps camlib_devel-repeater.eps 		\
	camlib_devel.sgml camlib_devel.txt camlib_devel-y.eps 		\
	frontend_devel.lyx frontend_devel.sgml frontend_devel.txt 	\
	libgphoto2.3 libgphoto2_port.3

man_MANS = libgphoto2.3 libgphoto2_port.3

# doc_DATA = FAQ faq.xml
########################################################################
# Conditional rules, depending on tool availability
########################################################################

if HAVE_DOXYGEN

DOXYGEN_FILES =
DOXYGEN_STAMPS = $(DOXYGEN_OUTPUT_DIR).stamp

# FIXME: Depending on the source files would be sufficient...
$(DOXYGEN_OUTPUT_DIR).stamp: Doxyfile $(top_builddir)/libgphoto2/libgphoto2.la
	doxygen Doxyfile
	echo > $@

$(top_builddir)/libgphoto2/libgphoto2.la:
	cd $(top_builddir)/libgphoto2 && $(MAKE) libgphoto2.la

$(HTML_APIDOC_DIR).tar.gz: $(DOXYGEN_OUTPUT_DIR).stamp
	(cd $(DOXYGEN_OUTPUT_DIR) && $(AMTAR) chof - $(HTML_APIDOC_DIR) | GZIP=--best gzip -c) > $@

DOC_APIDOC_HTML = apidocs.html

install-apidocs: $(DOXYGEN_OUTPUT_DIR).stamp
	$(INSTALL) -m 0755 -d $(DESTDIR)$(docdir)/$(DOC_APIDOC_HTML)
	$(INSTALL) -m 0644 $(DOXYGEN_OUTPUT_DIR)/$(HTML_APIDOC_DIR)/* $(DESTDIR)$(docdir)/$(DOC_APIDOC_HTML)/

uninstall-apidocs:
	rm -f $(DESTDIR)$(docdir)/$(DOC_APIDOC_HTML)/*
	rmdir $(DESTDIR)$(docdir)/$(DOC_APIDOC_HTML)

clean-apidocs:
	rm -rf $(DOXYGEN_OUTPUT_DIR)


# if we ever have more targets, move those out...

all-local: $(DOXYGEN_STAMPS)

install-data-local: install-apidocs

uninstall-local: uninstall-apidocs

clean-local: clean-apidocs

endif


########################################################################
# Common part
########################################################################

EXTRA_DIST = Doxyfile.in README.apidocs $(HTML_APIDOC_DIR).tar.gz $(old_EXTRA_DIST)

doc_DATA = $(DOXYGEN_FILES) README.apidocs


CLEANFILES = $(DOXYGEN_FILES) $(DOXYGEN_STAMPS)

