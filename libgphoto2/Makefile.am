lib_LTLIBRARIES = libgphoto2.la

CFLAGS= @CFLAGS@ \
	-Wall \
	-I$(top_srcdir)/include \
	-DCAMLIBS=\"$(pkglibdir)\" \
	-DDEBUG

libgphoto2_la_LDFLAGS = -version-info 0:0:0
libgphoto2_la_SOURCES = \
			core.c core.h \
			file.c file.h \
			library.c library.h \
			settings.c settings.h \
			util.c util.h
