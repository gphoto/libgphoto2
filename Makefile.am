lib_LTLIBRARIES = libgphoto2.la

CFLAGS= @CFLAGS@ \
	-Wall \
	-I$(top_srcdir)/include \
	-DCAMLIBS=\"$(pkglibdir)\"

libgphoto2_la_LDFLAGS = -version-info 0:0:0
libgphoto2_la_SOURCES = cache.c cache.h \
			core.c core.h \
			helpers.c helpers.h \
			library.c library.h \
			settings.c settings.h \
			util.c util.h
