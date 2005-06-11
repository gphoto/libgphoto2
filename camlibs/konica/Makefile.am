SUBDIRS = localization

camlib_LTLIBRARIES = konica.la konica_qm150.la

EXTRA_DIST = EXPERTS konica.df makefile.os2 qm150.txt

DEFS = \
	-DLOCALIZATION=\"$(datadir)/$(PACKAGE)/$(VERSION)/konica\"

konica_la_SOURCES = \
	lowlevel.c lowlevel.h \
	konica.c konica.h \
	library.c

konica_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
konica_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la

konica_qm150_la_SOURCES = qm150.c
konica_qm150_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
konica_qm150_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
