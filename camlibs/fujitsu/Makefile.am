camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_fujitsu.la

CFLAGS = @CFLAGS@ -g `gpio-config --cflags`
LDFLAGS = @LDFLAGS@ -g `gpio-config --libs`
INCLUDES = -I$(top_srcdir)/include

libgphoto2_fujitsu_la_SOURCES = fujitsu.c fujitsu.h

libgphoto2_fujitsu_la_LDFLAGS = -version-info 0:0:0 `gpio-config --libs`
