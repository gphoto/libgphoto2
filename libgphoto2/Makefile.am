lib_LTLIBRARIES = libgphoto2.la

CFLAGS= @CFLAGS@ \
	-I$(top_srcdir)/include \
	-DCAMLIBS=\"$(prefix)/lib/gphoto2\" \
	-DDOCDIR=\"$(prefix)/doc/gphoto2\"

libgphoto2_la_LDFLAGS = -version-info 0:0:0
libgphoto2_la_SOURCES = 	\
	abilities.c		\
	abilities-list.c	\
	bayer.c bayer.h		\
	camera.c		\
	core.c			\
	debug.c			\
	exif.c exif.h		\
	file.c			\
	filesys.c		\
	frontend.c		\
	gamma.c gamma.h		\
	jpeg.c jpeg.h		\
	list.c			\
	pattrec.c pattrec.h	\
	result.c		\
	setting.c		\
	widget.c
libgphoto2_la_LIBADD = -lm

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgphoto-2.0.pc
EXTRA_DIST = libgphoto-2.0.pc.in

