camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_jamcam.la

CFLAGS = @CFLAGS@ `glib-config --cflags`
LDFLAGS = @LDFLAGS@ -lm `glib-config --libs`
EXTRA_DIST =

INCLUDES =\
 -I$(top_srcdir)/include\
 -DG_LOG_DOMAIN=\"$(PACKAGE)\"\
 -I$(top_srcdir)/libgphoto2

libgphoto2_jamcam_la_SOURCES = \
 jamcam.c \
 library.c library.h

libgphoto2_jamcam_la_LDFLAGS =\
 -avoid-version\
 -L$(shell pwd)/$(top_builddir)/libgphoto2/@dotlibs@

libgphoto2_jamcam_la_LIBADD = -lgphoto2 -lm
