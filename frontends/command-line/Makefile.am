## Process this file with automake to create a GNU Makefile

bin_PROGRAMS = gphoto2

CFLAGS = @CFLAGS@ -g 

INCLUDES =						\
	-DGPHOTO2_LOCALEDIR=\""$(datadir)/locale"\"	\
	-I$(top_srcdir)/include 

LDFLAGS = @LDFLAGS@ -g 
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

gphoto2_LDADD = ../../libgphoto2/libgphoto2.la
