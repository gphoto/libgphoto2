camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_jamcam.la

CFLAGS = @CFLAGS@ `glib-config --cflags`
LDFLAGS = @LDFLAGS@ -lm `glib-config --libs`
EXTRA_DIST =

INCLUDES =                                              \
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2

libgphoto2_jamcam_la_SOURCES = jamcam.c library.c library.h
libgphoto2_jamcam_la_LDFLAGS = -module -avoid-version
libgphoto2_jamcam_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
