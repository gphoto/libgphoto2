camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_kodak_dc240.la

INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)/libgphoto2

libgphoto2_kodak_dc240_la_SOURCES = dc240.c dc240.h library.c library.h

libgphoto2_kodak_dc240_la_LDFLAGS =\
 -avoid-version\
 -L$(shell pwd)/$(top_builddir)/libgphoto2/@dotlibs@

libgphoto2_kodak_dc240_la_LIBADD = -lgphoto2
