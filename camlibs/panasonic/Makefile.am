SUBDIRS = coolshot l859

camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES =\
 libgphoto2_panasonic_dc1000.la\
 libgphoto2_panasonic_dc1580.la

INCLUDES = -I$(top_srcdir)/include  -I$(top_srcdir)/libgphoto2

libgphoto2_panasonic_dc1000_la_SOURCES = dc1000.c dc1000.h dc.c dc.h

libgphoto2_panasonic_dc1000_la_LDFLAGS =\
 -avoid-version\
 -L$(shell pwd)/$(top_builddir)/libgphoto2/@dotlibs@

libgphoto2_panasonic_dc1000_la_LIBADD = -lgphoto2

libgphoto2_panasonic_dc1580_la_SOURCES = dc1580.c dc1580.h dc.c dc.h dc1000.h

libgphoto2_panasonic_dc1580_la_LDFLAGS =\
 -avoid-version\
 -L$(shell pwd)/$(top_builddir)/libgphoto2/@dotlibs@

libgphoto2_panasonic_dc1580_la_LIBADD = -lgphoto2
