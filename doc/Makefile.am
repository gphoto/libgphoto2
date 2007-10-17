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

DOXYGEN_STAMPS =
LOCAL_INSTALL =
LOCAL_UNINSTALL =
LOCAL_CLEAN =


# FIXME: Depend on source files.
DOXYGEN_STAMPS += $(HTML_APIDOC_DIR).stamp
$(HTML_APIDOC_DIR).stamp: Doxyfile
	doxygen Doxyfile
	echo > $@

$(HTML_APIDOC_DIR).tar.gz: $(HTML_APIDOC_DIR).stamp
	(cd $(DOXYGEN_OUTPUT_DIR) && $(AMTAR) chof - $(HTML_APIDOC_DIR) | GZIP=--best gzip -c) > $@

$(HTML_APIDOC_INTERNALS_DIR).tar.gz: $(HTML_APIDOC_INTERNALS_DIR).stamp
	(cd $(DOXYGEN_OUTPUT_DIR) && $(AMTAR) chof - $(HTML_APIDOC_INTERNALS_DIR) | GZIP=--best gzip -c) > $@

LOCAL_INSTALL += install-apidocs
install-apidocs: $(HTML_APIDOC_DIR).stamp
	mkdir -p "$(DESTDIR)$(docdir)"
	find "$(DOXYGEN_OUTPUT_DIR)/$(HTML_APIDOC_DIR)" \
		\( -type d -exec chmod 0755 {} \; \) -or \
		\( -type f -exec chmod 0644 {} \; \)
	(unset CDPATH; cd "$(DOXYGEN_OUTPUT_DIR)" && tar cpf - "$(HTML_APIDOC_DIR)") \
		| (cd "$(DESTDIR)$(docdir)" && tar xpf -)

LOCAL_UNINSTALL += uninstall-apidocs
uninstall-apidocs:
	rm -rf "$(DESTDIR)$(docdir)/$(HTML_APIDOC_DIR)"

LOCAL_CLEAN += clean-apidocs
clean-apidocs:
	rm -rf "$(DOXYGEN_OUTPUT_DIR)/$(HTML_APIDOC_DIR)"
	rm -f $(HTML_APIDOC_DIR).stamp
	rm -f $(HTML_APIDOC_DIR).tar.gz

if ENABLE_INTERNAL_DOCS

DOXYGEN_STAMPS += $(HTML_APIDOC_INTERNALS_DIR).stamp
# FIXME: Depend on source files.
$(HTML_APIDOC_INTERNALS_DIR).stamp: Doxyfile-internals
	doxygen Doxyfile-internals
	echo > $@

LOCAL_INSTALL += install-apidocs-internals
install-apidocs-internals: $(HTML_APIDOC_INTERNALS_DIR).stamp
	mkdir -p "$(DESTDIR)$(docdir)"
	find "$(DOXYGEN_OUTPUT_DIR)/$(HTML_APIDOC_INTERNALS_DIR)" \
		\( -type d -exec chmod 0755 {} \; \) -or \
		\( -type f -exec chmod 0644 {} \; \)
	(unset CDPATH; cd "$(DOXYGEN_OUTPUT_DIR)" && tar cpf - "$(HTML_APIDOC_INTERNALS_DIR)") \
		| (cd "$(DESTDIR)$(docdir)" && tar xpf -)

LOCAL_UNINSTALL += uninstall-apidocs-internals
uninstall-apidocs-internals:
	rm -rf "$(DESTDIR)$(docdir)/$(HTML_APIDOC_INTERNALS_DIR)"

LOCAL_CLEAN += clean-apidocs-internals
clean-apidocs-internals:
	rm -rf "$(DOXYGEN_OUTPUT_DIR)/$(HTML_APIDOC_INTERNALS_DIR)"
	rm -f $(HTML_APIDOC_INTERNALS_DIR).stamp
	rm -f $(HTML_APIDOC_INTERNALS_DIR).tar.gz

endif

all-local: $(DOXYGEN_STAMPS)

install-data-local: $(LOCAL_INSTALL)

uninstall-local: $(LOCAL_UNINSTALL)

clean-local: $(LOCAL_CLEAN)
	rm -rf $(DOXYGEN_OUTPUT_DIR)

doc_EXTRA_DIST = $(HTML_APIDOC_DIR).tar.gz

doc_DATA = README.apidocs

CLEANFILES = $(DOXYGEN_STAMPS)

endif


########################################################################
# Common part
########################################################################

EXTRA_DIST = $(doc_EXTRA_DIST) Doxyfile.in README.apidocs $(old_EXTRA_DIST)
