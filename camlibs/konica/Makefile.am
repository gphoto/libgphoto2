SUBDIRS = localization

camlib_LTLIBRARIES = libgphoto2_konica.la libgphoto2_konica_qm150.la

EXTRA_DIST = EXPERTS konica.df makefile.os2 qm150.txt

DEFS = \
	-DLOCALIZATION=\"$(datadir)/$(PACKAGE)/$(VERSION)/konica\"

libgphoto2_konica_la_SOURCES = \
 lowlevel.c lowlevel.h \
 konica.c konica.h \
 library.c

libgphoto2_konica_qm150_la_SOURCES = qm150.c
libgphoto2_konica_qm150_la_LDFLAGS = -module -avoid-version
libgphoto2_konica_qm150_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la

libgphoto2_konica_la_LDFLAGS = -module -avoid-version
libgphoto2_konica_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
