lib_LTLIBRARIES = libgphoto2_frontend.la

CFLAGS= @CFLAGS@ \
	-g \
	-Wall \
	-I$(top_srcdir)/include \
	-DCAMLIBS=\"$(pkglibdir)\"
LDFLAGS = @LDFLAGS@ -g

libgphoto2_frontend_la_LDFLAGS = -version-info 0:0:0
libgphoto2_frontend_la_SOURCES = \
			cache.c cache.h \
			helpers.c helpers.h \
			image.c image.h
