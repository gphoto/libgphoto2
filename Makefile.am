lib_LTLIBRARIES = libgphoto2.la

CFLAGS= @CFLAGS@ \
	-I$(top_srcdir)/include \
	-DCAMLIBS=\"$(prefix)/lib/gphoto2\" \
	-DDOCDIR=\"@DOC_DIR@\"

libgphoto2_la_LDFLAGS = -version-info 0:0:0
libgphoto2_la_SOURCES = 	\
	gphoto2-abilities-list.c\
	bayer.c bayer.h		\
	gphoto2-camera.c	\
	gphoto2-core.c		\
	gphoto2-debug.c		\
	exif.c exif.h		\
	file.c			\
	gphoto2-filesys.c	\
	gamma.c gamma.h		\
	jpeg.c jpeg.h		\
	gphoto2-list.c		\
	pattrec.c pattrec.h	\
	gphoto2-result.c	\
	setting.c		\
	gphoto2-widget.c
libgphoto2_la_LIBADD = -lm

libgphoto2includedir = $(includedir)/gphoto2
libgphoto2include_HEADERS =	\
	gphoto2-abilities.h	\
	gphoto2-abilities-list.h\
	gphoto2-camera.h	\
	gphoto2-core.h		\
	gphoto2-debug.h		\
	gphoto2-filesys.h	\
	gphoto2-library.h	\
	gphoto2-list.h		\
	gphoto2-result.h	\
	gphoto2-widget.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgphoto-2.0.pc
EXTRA_DIST = libgphoto-2.0.pc.in

