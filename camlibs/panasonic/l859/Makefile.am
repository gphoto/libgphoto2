camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_panasonic_l859.la

INCLUDES = -I$(top_srcdir)/include

libgphoto2_panasonic_l859_la_SOURCES = l859.c l859.h

libgphoto2_panasonic_l859_la_LDFLAGS =\
 -avoid-version\
 -L$(shell pwd)/$(top_builddir)/libgphoto2/.libs

libgphoto2_panasonic_l859_la_LIBADD = -lgphoto2
