## Process this file with automake to produce Makefile.in

# The name of the module.
DOC_MODULE=libgpio

# The top-level SGML file.
DOC_MAIN_SGML_FILE=libgpio.sgml

# The directory containing the source code (if it contains documentation).
DOC_SOURCE_DIR=..

HTML_DIR=$(prefix)/share/doc/libgpio$(LIBGPIO_MAJOR_VERSION).$(LIBGPIO_MINOR_VERSION)-dev/html

TARGET_DIR=$(HTML_DIR)/$(DOC_MODULE)


scan:
	gtkdoc-scan --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR) --ignore-headers="acconfig.h config.h examples/jmtransfer.h src/libgpio-private.h src/ftp.h"

templates: scan
	gtkdoc-mktmpl --module=$(DOC_MODULE)

sgml:
	gtkdoc-mkdb --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR)

html:
	if ! test -d html ; then mkdir html ; fi
	-cd html && gtkdoc-mkhtml $(DOC_MODULE) ../$(DOC_MAIN_SGML_FILE)

clean-local:
	rm -f *~ *.bak *.hierarchy *.signals *-unused.txt

maintainer-clean-local: clean
	rm -rf sgml html $(DOC_MODULE)-decl-list.txt $(DOC_MODULE)-decl.txt 

libgpio-decl-list.txt : templates

libgpio-sections.txt : scan
	cp libgpio-decl-list.txt libgpio-sections.txt

rebuild: libgpio-sections.txt templates sgml html

install-data-local: 
	$(mkinstalldirs) $(DESTDIR)$(TARGET_DIR)
	-@INSTALL@ -m 0644 $(srcdir)/html/*.html $(DESTDIR)$(TARGET_DIR)
	-@INSTALL@ -m 0644 $(srcdir)/html/index.sgml $(DESTDIR)$(TARGET_DIR)
	-(cd $(DESTDIR); gtkdoc-fixxref --module=$(DOC_MODULE) --html-dir=$(HTML_DIR))
	-mv $(HTML_DIR)/$(DOC_MODULE)/* $(HTML_DIR)
	-rm -rf $(HTML_DIR)/$(DOC_MODULE)
	-(cd $(HTML_DIR); ln -s book1.html index.html)

dist-hook:
	(cd $(srcdir) ; tar cvf - html/*.html html/*.sgml) | (cd $(distdir); tar xf -)

copyweb:
	rm -rf $(HOME)/public_html/libgpio/docs/*
	cp -f html/*.html $(HOME)/public_html/libgpio/docs
	cp -f $(HOME)/public_html/libgpio/docs/book1.html $(HOME)/public_html/libgpio/docs/index.html
	chmod -R a+rX $(HOME)/public_html/libgpio/docs

.PHONY : html sgml templates scan
