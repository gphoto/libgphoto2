SUBDIRS = doc

EXTRA_DIST = .indent.pro Changelog canon.df makefile.os2 mkpatch Protocol

camlibdir = $(libdir)/gphoto2/$(VERSION)
camlib_LTLIBRARIES = libgphoto2_canon.la

INCLUDES =                                              \
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2

# Uncomment the CFLAGS line if you want to play around with the
# experimental capture and/or upload stuff
#
# NOTE: This may CAUSE DAMAGE TO THE CAMERA!
#
# You may have to do a "make clean install" in this directory in order
# for these features to work.
#
# CFLAGS += -DEXPERIMENTAL_CAPTURE -DEXPERIMENTAL_UPLOAD

libgphoto2_canon_la_SOURCES =\
 canon.c canon.h\
 library.c library.h\
 serial.c serial.h \
 usb.c usb.h \
 crc.c crc.h \
 util.c util.h

libgphoto2_canon_la_LDFLAGS = -module -avoid-version
libgphoto2_canon_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
