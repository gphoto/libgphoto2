## Process this file with automake to create a GNU Makefile

bin_PROGRAMS = gphoto2

CFLAGS = @CFLAGS@ -I$(top_srcdir)/include

gphoto2_SOURCES = main.c main.h \
		interface.c interface.h

gphoto2_LDADD = ../../libgphoto2/libgphoto2.la \
		../libgphoto2_util/libgphoto2_util.la
