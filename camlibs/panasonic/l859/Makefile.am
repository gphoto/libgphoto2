camlibdir = $(libdir)/gphoto2/$(VERSION)
camlib_LTLIBRARIES = libgphoto2_panasonic_l859.la

INCLUDES =                                              \
        -I$(top_srcdir)/intl				\
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2

libgphoto2_panasonic_l859_la_SOURCES = l859.c l859.h
libgphoto2_panasonic_l859_la_LDFLAGS = -module -avoid-version
libgphoto2_panasonic_l859_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
