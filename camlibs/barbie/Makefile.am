camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_barbie.la

INCLUDES = -I$(top_srcdir)/include

libgphoto2_barbie_la_SOURCES = barbie.c barbie.h library.c library.h

libgphoto2_barbie_la_LDFLAGS =\
 -avoid-version\
 -L$(shell pwd)/$(top_builddir)/libgphoto2/@dotlibs@

libgphoto2_barbie_la_LIBADD = -lgphoto2
