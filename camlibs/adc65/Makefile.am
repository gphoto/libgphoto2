camlib_LTLIBRARIES = libgphoto2_adc65.la
EXTRA_DIST = README TODO Changelog

INCLUDES =						\
        -I$(top_srcdir)/intl				\
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port	\
        -I$(top_srcdir)/libgphoto2

libgphoto2_adc65_la_SOURCES = adc65.c adc65.h library.c
libgphoto2_adc65_la_LDFLAGS = -module -avoid-version
libgphoto2_adc65_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
