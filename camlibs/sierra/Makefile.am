camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_sierra.la

CFLAGS = @CFLAGS@ -g `gpio-config --cflags`
LDFLAGS = @LDFLAGS@ -g `gpio-config --libs`
INCLUDES = -I$(top_srcdir)/include

libgphoto2_sierra_la_SOURCES = sierra.c sierra.h library.c library.h

libgphoto2_sierra_la_LDFLAGS = -version-info 0:0:0 `gpio-config --libs`
