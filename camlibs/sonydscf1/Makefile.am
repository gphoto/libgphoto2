camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_sonydscf1.la

CFLAGS = @CFLAGS@ -g `gpio-config --cflags`
LDFLAGS = @LDFLAGS@ -g `gpio-config --libs`
INCLUDES = -I$(top_srcdir)/include

libgphoto2_sonydscf1_la_SOURCES = chotplay.c chotplay.h command.c command.h \
	common.h config.h getuint.c getuint.h pmp.h sonydscf1.c sonydscf1.h

libgphoto2_sonydscf1_la_LDFLAGS = -version-info 0:0:0

