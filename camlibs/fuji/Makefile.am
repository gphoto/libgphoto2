camlibdir = $(libdir)/gphoto2/$(VERSION)
camlib_LTLIBRARIES = libgphoto2_fuji.la

INCLUDES =                                              \
        -I$(top_srcdir)/intl				\
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2

libgphoto2_fuji_la_SOURCES = library.c fuji.c fuji.h
libgphoto2_fuji_la_LDFLAGS = -module -avoid-version
libgphoto2_fuji_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
