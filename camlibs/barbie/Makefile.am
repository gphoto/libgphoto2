camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_barbie.la

CFLAGS = @CFLAGS@ -g `gpio-config --cflags`
LDFLAGS = @LDFLAGS@ -g `gpio-config --libs`
INCLUDES = -I$(top_srcdir)/include

libgphoto2_barbie_la_SOURCES = barbie.c barbie.h library.c library.h

libgphoto2_barbie_la_LDFLAGS = -version-info 0:0:0
