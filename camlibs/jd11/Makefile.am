EXTRA_DIST = jd11.html

camlibdir = $(libdir)/gphoto2/$(VERSION)
camlib_LTLIBRARIES = libgphoto2_jd11.la

INCLUDES =                                              \
        -I$(top_srcdir)/intl				\
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2

libgphoto2_jd11_la_SOURCES = jd11.c serial.h serial.c decomp.h decomp.c
libgphoto2_jd11_la_LDFLAGS = -module -avoid-version
libgphoto2_jd11_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
