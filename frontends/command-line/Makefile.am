## Process this file with automake to create a GNU Makefile

bin_PROGRAMS = gphoto2

CFLAGS = @CFLAGS@   -g `gpio-config --cflags` -I$(top_srcdir)/include
LDFLAGS = @LDFLAGS@ -g `gpio-config --libs`

gphoto2_SOURCES = \
		main.c main.h \
		interface.c interface.h \
		test.c test.h

gphoto2_LDADD = ../../libgphoto2/libgphoto2.la \
		../libgphoto2_frontend/libgphoto2_frontend.la
