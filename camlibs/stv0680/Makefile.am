camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_stv0680.la

INCLUDES = -I$(top_srcdir)/include

libgphoto2_stv0680_la_SOURCES = stv0680.c stv0680.h library.c library.h bayer.c bayer.h pattrec.c pattrec.h

libgphoto2_stv0680_la_LDFLAGS =\
 -avoid-version\
 -L$(shell pwd)/$(top_builddir)/libgphoto2/.libs

libgphoto2_stv0680_la_LIBADD = -lgphoto2
