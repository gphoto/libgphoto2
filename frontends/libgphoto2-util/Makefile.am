lib_LTLIBRARIES = libgphoto2-util.la

CFLAGS= @CFLAGS@ \
	-Wall \
	-I$(top_srcdir)/include \
	-DCAMLIBS=\"$(pkglibdir)\"

libgphoto2-util_la_LDFLAGS = -version-info 0:0:0
libgphoto2-util_la_SOURCES = \
			cache.c cache.h
