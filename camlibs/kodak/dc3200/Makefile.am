EXTRA_DIST = kodak.df makefile.os2

camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_kodak_dc3200.la

INCLUDES =                                              \
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2

libgphoto2_kodak_dc3200_la_SOURCES = dc3200.c dc3200.h library.c library.h

libgphoto2_kodak_dc3200_la_LDFLAGS =\
 -module -avoid-version\
 -L$(shell pwd)/$(top_builddir)/libgphoto2/@dotlibs@

libgphoto2_kodak_dc3200_la_LIBADD = -lgphoto2
