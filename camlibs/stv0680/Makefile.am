EXTRA_DIST = stv0680.df makefile.os2 CREDITS LICENCE stv0680.html

camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_stv0680.la

INCLUDES = -I$(top_srcdir)/include  -I$(top_srcdir)/libgphoto2

libgphoto2_stv0680_la_SOURCES = stv0680.c stv0680.h library.c library.h bayer.c bayer.h sharpen.c

libgphoto2_stv0680_la_LDFLAGS =\
 -module -avoid-version\
 -L$(shell pwd)/$(top_builddir)/libgphoto2/@dotlibs@

libgphoto2_stv0680_la_LIBADD = -lgphoto2
