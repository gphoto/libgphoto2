camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_template.la

CFLAGS = @CFLAGS@ `gpio-config --cflags`
LDFLAGS = @LDFLAGS@ `gpio-config --libs`
INCLUDES = -I$(top_srcdir)/include

libgphoto2_template_la_SOURCES = template.c template.h

libgphoto2_template_la_LDFLAGS = -version-info 0:0:0 `gpio-config --libs`
