lib_LTLIBRARIES = libgphoto2_frontend.la

gphotoheaddir = $(prefix)/include/gphoto2
gphotohead_HEADERS = gphoto2-frontend.h

CFLAGS= @CFLAGS@ \
	-g \
	-Wall \
	-I$(top_srcdir)/include \
	-DCAMLIBS=\"$(pkglibdir)\"
LDFLAGS = @LDFLAGS@ -g

libgphoto2_frontend_la_LDFLAGS = -version-info 0:0:0
libgphoto2_frontend_la_SOURCES = \
			gphoto2-frontend.h \
			cache.c \
			helpers.c \
			image.c \
			script.c
