lib_LTLIBRARIES = libgphoto2.la

CFLAGS= @CFLAGS@ \
	-I$(top_srcdir)/include \
	-DCAMLIBS=\"$(prefix)/lib/gphoto2\" \
	-DDOCDIR=\"$(prefix)/doc/gphoto2\"

libgphoto2_la_LDFLAGS = -version-info 0:0:0
libgphoto2_la_SOURCES = \
	camera.c \
	core.c core.h \
	exif.c exif.h \
	file.c \
	filesys.c \
	frontend.c \
	globals.h \
	library.c library.h \
	lists.c \
	port.c \
	settings.c settings.h \
	util.c util.h \
	widget.c

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgphoto-2.0.pc
EXTRA_DIST = libgphoto-2.0.pc.in
