EXTRA_DIST = CREDITS LICENCE

camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_jd350e.la

INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)/libgphoto2

libgphoto2_jd350e_la_SOURCES = jd350e.c jd350e.h library.c library.h bayer.c bayer.h redcurve.h

libgphoto2_jd350e_la_LDFLAGS =\
 -module -avoid-version\
 -L$(shell pwd)/$(top_builddir)/libgphoto2/@dotlibs@

libgphoto2_jd350e_la_LIBADD = -lgphoto2
