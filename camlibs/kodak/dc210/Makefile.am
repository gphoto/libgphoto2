camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_kodak_dc210.la

INCLUDES = -I$(top_srcdir)/include

libgphoto2_kodak_dc210_la_SOURCES = dc210.c dc210.h library.c library.h

libgphoto2_kodak_dc210_la_LDFLAGS =\
 -avoid-version\
 -L$(shell pwd)/$(top_builddir)/libgphoto2/@dotlibs@

libgphoto2_kodak_dc210_la_LIBADD = -lgphoto2
