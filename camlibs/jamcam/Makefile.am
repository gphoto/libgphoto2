camlibdir = $(libdir)/gphoto2/$(VERSION)
camlib_LTLIBRARIES = libgphoto2_jamcam.la

INCLUDES =                                              \
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2

libgphoto2_jamcam_la_SOURCES = jamcam.c library.c library.h
libgphoto2_jamcam_la_LDFLAGS = -module -avoid-version
libgphoto2_jamcam_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
