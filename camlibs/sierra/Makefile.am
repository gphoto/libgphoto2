camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_sierra.la

INCLUDES = -I$(top_srcdir)/include  -I$(top_srcdir)/libgphoto2

libgphoto2_sierra_la_SOURCES =\
 sierra.c sierra.h\
 library.c library.h\
 sierra-usbwrap.c sierra-usbwrap.h

libgphoto2_sierra_la_LDFLAGS =\
 -avoid-version\
 -L$(shell pwd)/$(top_builddir)/libgphoto2/@dotlibs@

libgphoto2_sierra_la_LIBADD = -lgphoto2
