SUBDIRS = localization

camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_konica.la

EXTRA_DIST = EXPERTS konica.df makefile.os2

INCLUDES =\
 -I$(top_srcdir)/include\
 -DLOCALIZATION=\"$(datadir)/gphoto2/konica\"\
 -I$(top_srcdir)/libgphoto2

libgphoto2_konica_la_SOURCES = \
 lowlevel.c lowlevel.h \
 konica.c konica.h \
 library.c library.h

libgphoto2_konica_la_LDFLAGS =\
 -module -avoid-version\
 -L$(shell pwd)/$(top_builddir)/libgphoto2/@dotlibs@

libgphoto2_konica_la_LIBADD = -lgphoto2
