camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_sony_dscf55.la

INCLUDES = -I$(top_srcdir)/include  -I$(top_srcdir)/libgphoto2

libgphoto2_sony_dscf55_la_SOURCES = \
	sony.c sony.h camera.c nls.h

libgphoto2_sony_dscf55_la_LDFLAGS =\
 -avoid-version\
 -L$(shell pwd)/$(top_builddir)/libgphoto2/@dotlibs@

libgphoto2_sony_dscf55_la_LIBADD = -lgphoto2
