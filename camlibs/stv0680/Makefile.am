EXTRA_DIST = stv0680.df makefile.os2 CREDITS LICENCE stv0680.html

camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_stv0680.la

INCLUDES =                                              \
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2

libgphoto2_stv0680_la_SOURCES = stv0680.c stv0680.h library.c library.h bayer.c bayer.h

libgphoto2_stv0680_la_LDFLAGS = -module -avoid-version
libgphoto2_stv0680_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
