DOC_MODULE=gphoto2

DOC_MAIN_SGML_FILE=gphoto2-api.sgml

DOC_SOURCE_DIR=$(top_srcdir)/libgphoto2

CFLAGS="-I../.. `gphoto2-config --cflags`"
LDFLAGS="`gphoto2-config --ldflags`"

TARGET_DIR=$(HTML_DIR)/$(DOC_MODULE)

tmpl_sources =			\
	tmpl/widget.sgml

gphoto2_docdir = $(HTML_DIR)
gphoto2_doc_DATA = 		\
	gphoto2-api.sgml	\
	gphoto2-api.html

EXTRA_DIST = $(gphoto2_doc_DATA)

if ENABLE_GTK_DOC
gphoto2-api.html: html/book1.html
	-cp $(srcdir)/html/book1.html $(srcdir)/gphoto2-api.html
else
gphoto2-api.html:
endif

html/book1.html: sgml/gphoto2-doc.bottom
	$(MAKE) html

sgml/gphoto2-doc.bottom: $(tmpl_sources)
	$(MAKE) sgml

scan:
	gtkdoc-scan --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR)

templates:
	cd $(srcdir) && gtkdoc-mktmpl --module=$(DOC_MODULE)

sgml:
	cd $(srcdir) \
	&& gtkdoc-mkdb --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR)

html:
	test -d $(srcdir)/html || mkdir $(srcdir)/html
	-cd $(srcdir)/html && gtkdoc-mkhtml $(DOC_MODULE) ../$(DOC_MAIN_SGML_FILE)

clean-local:
	rm -f *~ *.bak *-unused.txt

maintainer-clean-local: clean
	cd $(srcdir) && rm -rf sgml html $(DOC_MODULE)-decl-list.txt $(DOC_MODULE)-decl.txt

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(TARGET_DIR)
	(installfiles=`echo $(srcdir)/html/*.html`; \
	if test "$$installfiles" = '$(srcdir)/html/*.html'; \
	then echo '-- Nothing to install' ; \
	else \
	  for i in $$installfiles; do \
	    echo '-- Installing '$$i ; \
	    $(INSTALL_DATA) $$i $(DESTDIR)$(TARGET_DIR); \
	  done; \
	  echo '-- Installing $(srcdir)/html/index.sgml' ; \
	  $(INSTALL_DATA) $(srcdir)/html/index.sgml $(DESTDIR)$(TARGET_DIR); \
	  echo '-- Fixing Crossreferences' ; \
	  gtkdoc-fixxref --module=$(DOC_MODULE) --html-dir=$(HTML_DIR)|| true; \
	fi)

dist-hook:
	mkdir $(distdir)/html
	mkdir $(distdir)/sgml
	mkdir $(distdir)/tmpl
	-cp $(srcdir)/html/*.html $(srcdir)/html/*.css $(distdir)/html
	-cp $(srcdir)/tmpl/*.sgml $(distdir)/tmpl
	-cp $(srcdir)/sgml/*.sgml $(distdir)/sgml
	-cp $(srcdir)/sgml/*.bottom $(srcdir)/sgml/*.top $(distdir)/sgml

.PHONY : html sgml templates scan

		      

