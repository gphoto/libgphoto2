camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_dimagev.la

INCLUDES = -I$(top_srcdir)/include

libgphoto2_dimagev_la_SOURCES = capture.c data.c delete.c dimagev.c dimagev.h \
	download.c info.c packet.c status.c util.c

libgphoto2_dimagev_la_LDFLAGS = -version-info 0:0:0

libgphoto2_dimagev_la_LIBADD = -lgphoto2
