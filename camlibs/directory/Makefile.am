camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_directory.la

INCLUDES = -I$(top_srcdir)/include
CFLAGS = @CFLAGS@ -g `gpio-config --cflags`
LDFLAGS = @LDFLAGS@ -g `gpio-config --libs`

libgphoto2_directory_la_SOURCES = directory.c directory.h

libgphoto2_directory_la_LDFLAGS = -version-info 0:0:0
