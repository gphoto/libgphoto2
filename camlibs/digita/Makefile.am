camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_digita.la

CFLAGS = @CFLAGS@ -g `gpio-config --cflags`
LDFLAGS = @LDFLAGS@ -g `gpio-config --libs`
INCLUDES = -I$(top_srcdir)/include

libgphoto2_digita_la_SOURCES = digita.c serial.c usb.c commands.c digita.h

libgphoto2_digita_la_LDFLAGS = -version-info 0:0:0

