camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_dimagev.la

INCLUDES = -I$(top_srcdir)/include

libgphoto2_dimagev_la_SOURCES = dimagev.c dimagev.h packet.c data.c status.c \
	info.c capture.c util.c delete.c

libgphoto2_dimagev_la_LDFLAGS = -version-info 0:0:0
