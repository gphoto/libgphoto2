camlibdir = $(prefix)/lib/gphoto2/$(VERSION)
camlib_LTLIBRARIES = libgphoto2_fuji.la

INCLUDES =                                              \
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2

libgphoto2_fuji_la_SOURCES = fuji.c fuji.h fuji_exif.c exif.c
libgphoto2_fuji_la_LDFLAGS = -module -avoid-version
libgphoto2_fuji_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
