EXTRA_DIST = .indent.pro Changelog canon.df makefile.os2 mkpatch Protocol

camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_canon.la

CFLAGS = @CFLAGS@ -g -Wall
LDFLAGS = @LDFLAGS@ -g
INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)/libgphoto2

libgphoto2_canon_la_SOURCES =\
 canon.c canon.h\
 psa50.c psa50.h\
 serial.c serial.h \
 crc.c crc.h \
 util.c util.h

libgphoto2_canon_la_LDFLAGS =\
 -module -avoid-version -g\
 -L$(shell pwd)/$(top_builddir)/libgphoto2/@dotlibs@

libgphoto2_canon_la_LIBADD = -lgphoto2
