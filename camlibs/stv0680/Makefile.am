camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_stv0680.la

INCLUDES = -I$(top_srcdir)/include  -I$(top_srcdir)/libgphoto2

libgphoto2_stv0680_la_SOURCES = stv0680.c stv0680.h library.c library.h bayer.c bayer.h

libgphoto2_stv0680_la_LDFLAGS =\
 -avoid-version\
 -L$(shell pwd)/$(top_builddir)/libgphoto2/@dotlibs@

libgphoto2_stv0680_la_LIBADD = -lgphoto2
