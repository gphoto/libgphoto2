camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_canon.la

CFLAGS = @CFLAGS@ `gpio-config --cflags` -g -Wall
LDFLAGS = @LDFLAGS@ -g `gpio-config --libs`
INCLUDES = -I$(top_srcdir)/include

libgphoto2_canon_la_SOURCES =\
 canon.c canon.h\
 psa50.c psa50.h\
 serial.c serial.h \
 crc.c crc.h\
 util.c util.h

libgphoto2_canon_la_LDFLAGS =\
 -avoid-version -g\
 -L$(shell pwd)/$(top_builddir)/libgphoto2/.libs

libgphoto2_canon_la_LIBADD = -lgphoto2
