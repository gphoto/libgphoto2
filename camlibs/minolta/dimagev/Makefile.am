camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_dimagev.la

INCLUDES = -I$(top_srcdir)/include

libgphoto2_dimagev_la_SOURCES = capture.c data.c delete.c dimagev.c dimagev.h \
	download.c info.c packet.c status.c util.c

libgphoto2_dimagev_la_LDFLAGS =\
 -avoid-version\
 -L$(shell pwd)/$(top_builddir)/libgphoto2/.libs

libgphoto2_dimagev_la_LIBADD = -lgphoto2
