EXTRA_DIST = jpeg.txt jpeghead.h

camlibdir = $(libdir)/gphoto2/$(VERSION)
camlib_LTLIBRARIES =			\
	libgphoto2_polaroid_pdc320.la	\
	libgphoto2_polaroid_pdc640.la	\
	libgphoto2_polaroid_pdc700.la

INCLUDES =                                              \
        -I$(top_srcdir)/intl				\
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2			\
	-I$(top_builddir)/libgphoto2

libgphoto2_polaroid_pdc320_la_SOURCES = pdc320.c pdc320.h
libgphoto2_polaroid_pdc640_la_SOURCES = pdc640.c jd350e.h jd350e.c jd350e_red.h jd350e_blue.h
libgphoto2_polaroid_pdc700_la_SOURCES = pdc700.c

libgphoto2_polaroid_pdc320_la_LDFLAGS = -module -avoid-version
libgphoto2_polaroid_pdc640_la_LDFLAGS = -module -avoid-version
libgphoto2_polaroid_pdc700_la_LDFLAGS = -module -avoid-version

libgphoto2_polaroid_pdc320_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
libgphoto2_polaroid_pdc640_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
libgphoto2_polaroid_pdc700_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
