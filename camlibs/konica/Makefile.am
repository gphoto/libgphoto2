camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_konica.la

CFLAGS = @CFLAGS@ `glib-config --cflags`
LDFLAGS = @LDFLAGS@ `glib-config --libs`
EXTRA_DIST = ChangeLog EXPERTS

INCLUDES =\
 -I$(top_srcdir)/include\
 -DG_LOG_DOMAIN=\"$(PACKAGE)\"

libgphoto2_konica_la_SOURCES = \
 lowlevel.c lowlevel.h \
 konica.c konica.h \
 library.c library.h

libgphoto2_konica_la_LDFLAGS =\
 -avoid-version\
 -L$(shell pwd)/$(top_builddir)/libgphoto2/.libs

libgphoto2_konica_la_LIBADD = -lgphoto2
