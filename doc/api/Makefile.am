## Process this file with automake to produce Makefile.in

# The name of the module.
DOC_MODULE=gphoto2

# The top-level SGML file.
DOC_MAIN_SGML_FILE=gphoto2-api.sgml

# The directory containing the source code (if it contains documentation).
DOC_SOURCE_DIR=$(top_srcdir)/libgphoto2

EXTRA_DIST = gphoto2-api.sgml gphoto2-sections.txt

if ENABLE_GTK_DOC
htmldir = @HTML_DIR@

TARGET_DIR=$(htmldir)/$(DOC_MODULE)

all-local: templates sgml html

scan:
	cd $(srcdir) && gtkdoc-scan --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR)

templates: scan
	cd $(srcdir) && gtkdoc-mktmpl --module=$(DOC_MODULE)

sgml:
	gtkdoc-mkdb --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR)

html:
	if ! test -d html ; then mkdir html ; fi
	-cd html && gtkdoc-mkhtml $(DOC_MODULE) ../$(DOC_MAIN_SGML_FILE)
endif

clean-local:
	rm -f *~ *.bak *.hierarchy *.signals *.args *-unused.txt

maintainer-clean-local: clean
	rm -rf sgml html $(DOC_MODULE)-decl-list.txt $(DOC_MODULE)-decl.txt 

if ENABLE_GTK_DOC
install-data-local:
	$(INSTALL) -d -m 0755 $(DESTDIR)$(TARGET_DIR)
	$(INSTALL_DATA) $(srcdir)/html/*.html $(DESTDIR)$(TARGET_DIR)
	$(INSTALL_DATA) $(srcdir)/html/index.sgml $(DESTDIR)$(TARGET_DIR)
	gtkdoc-fixxref --module=$(DOC_MODULE) --html-dir=$(DESTDIR)$(htmldir)

dist-hook:
	(cd $(srcdir) ; tar cvf - html) | (cd $(distdir); tar xf -)

.PHONY : html sgml templates scan
endif
