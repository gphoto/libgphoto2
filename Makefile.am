lib_LTLIBRARIES = libgphoto2.la

CFLAGS= @CFLAGS@ \
	-g \
	-Wall \
	-I$(top_srcdir)/include \
	-DCAMLIBS=\"$(pkglibdir)\" \
	-DDEBUG
LDFLAGS = @LDFLAGS@ -g

libgphoto2_la_LDFLAGS = -version-info 0:0:0
libgphoto2_la_SOURCES = \
			core.c core.h \
			exif.c exif.h \
			file.c file.h \
			globals.h \
			library.c library.h \
			settings.c settings.h \
			util.c util.h
