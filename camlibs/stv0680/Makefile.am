EXTRA_DIST = stv0680.df makefile.os2 CREDITS LICENCE README.pdf 680_commands_and_communication_sdk.pdf.gz

camlibdir = $(prefix)/lib/gphoto2/$(VERSION)
camlib_LTLIBRARIES = libgphoto2_stv0680.la

INCLUDES =                                              \
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2

libgphoto2_stv0680_la_SOURCES = stv0680.c stv0680.h library.c library.h bayer.c bayer.h sharpen.c sharpen.h saturate.c saturate.h

libgphoto2_stv0680_la_LDFLAGS = -module -avoid-version
libgphoto2_stv0680_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
