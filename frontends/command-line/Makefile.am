## Process this file with automake to create a GNU Makefile

bin_PROGRAMS = gphoto2

CFLAGS = @CFLAGS@   -g `gpio-config --cflags` -I$(top_srcdir)/include -I../libgphoto2_frontend
LDFLAGS = @LDFLAGS@ -g `gpio-config --libs`

gphoto2_SOURCES = 		\
	actions.c actions.h 	\
	foreach.c foreach.h 	\
	globals.h 		\
	interface.c interface.h \
	main.c main.h 		\
	options.c options.h 	\
	range.c range.h 	\
	shell.c shell.h 	\
	test.c test.h

gphoto2_LDFLAGS = -export-dynamic

gphoto2_LDADD = ../../libgphoto2/libgphoto2.la \
		../libgphoto2_frontend/libgphoto2_frontend.la
