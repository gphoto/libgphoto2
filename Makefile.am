lib_LTLIBRARIES = libgphoto2.la

CFLAGS= @CFLAGS@ \
	-I$(top_srcdir)/include \
	-DCAMLIBS=\"$(prefix)/lib/gphoto2\" \
	-DDOCDIR=\"$(prefix)/doc/gphoto2\"

libgphoto2_la_LDFLAGS = -version-info 0:0:0
libgphoto2_la_SOURCES = 	\
	abilities.c		\
	abilities-list.c	\
	camera.c		\
	core.c			\
	exif.c exif.h		\
	file.c			\
	filesys.c		\
	frontend.c		\
	list.c			\
	result.c		\
	setting.c		\
	widget.c

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgphoto-2.0.pc
EXTRA_DIST = libgphoto-2.0.pc.in
