EXTRA_DIST = barbie.df makefile.os2 main.c compile

camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_barbie.la

INCLUDES =                                              \
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2

libgphoto2_barbie_la_SOURCES = barbie.c barbie.h library.c library.h

libgphoto2_barbie_la_LDFLAGS =\
 -module -avoid-version\
 -L$(shell pwd)/$(top_builddir)/libgphoto2/@dotlibs@

libgphoto2_barbie_la_LIBADD = -lgphoto2
