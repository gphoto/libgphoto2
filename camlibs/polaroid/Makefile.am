EXTRA_DIST = jpeg.txt jpeghead.h

camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES =			\
	libgphoto2_polaroid_pdc320.la	\
	libgphoto2_polaroid_pdc640.la	\
	libgphoto2_polaroid_pdc700.la

INCLUDES = -I$(top_srcdir)/include  -I$(top_srcdir)/libgphoto2

libgphoto2_polaroid_pdc320_la_SOURCES = pdc320.c pdc320.h
libgphoto2_polaroid_pdc640_la_SOURCES = pdc640.c jd350e.h jd350e.c jd350e_red.h jd350e_blue.h
libgphoto2_polaroid_pdc700_la_SOURCES = pdc700.c

libgphoto2_polaroid_pdc320_la_LDFLAGS =\
 -module -avoid-version\
 -L$(shell pwd)/$(top_builddir)/libgphoto2/@dotlibs@

libgphoto2_polaroid_pdc640_la_LDFLAGS =\
 -module -avoid-version\
 -L$(shell pwd)/$(top_builddir)/libgphoto2/@dotlibs@

libgphoto2_polaroid_pdc700_la_LDFLAGS =\
 -module -avoid-version\
 -L$(shell pwd)/$(top_builddir)/libgphoto2/@dotlibs@

libgphoto2_polaroid_pdc320_la_LIBADD = -lgphoto2
libgphoto2_polaroid_pdc640_la_LIBADD = -lgphoto2
libgphoto2_polaroid_pdc700_la_LIBADD = -lgphoto2
