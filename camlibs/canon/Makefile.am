SUBDIRS = doc

EXTRA_DIST = .indent.pro Changelog canon.df makefile.os2 mkpatch Protocol

camlibdir = $(prefix)/lib/gphoto2/$(VERSION)
camlib_LTLIBRARIES = libgphoto2_canon.la

CFLAGS = @CFLAGS@ -g -Wall
LDFLAGS = @LDFLAGS@ -g
INCLUDES =                                              \
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2

libgphoto2_canon_la_SOURCES =\
 canon.c canon.h\
 library.c library.h\
 serial.c serial.h \
 usb.c usb.h \
 crc.c crc.h \
 util.c util.h

libgphoto2_canon_la_LDFLAGS = -module -avoid-version
libgphoto2_canon_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
