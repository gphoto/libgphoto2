INCLUDES =						\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/libgphoto2_port/libgphoto2_port	\
	-DCAMLIBS=\"$(prefix)/lib/gphoto2/$(VERSION)\"	\
	-DLOCALEDIR=\"$(datadir)/locale\"

lib_LTLIBRARIES = libgphoto2.la

libgphoto2_la_LDFLAGS = -version-info 0:0:0
libgphoto2_la_SOURCES = 	\
	gphoto2-abilities-list.c\
	bayer.c bayer.h		\
	gphoto2-camera.c	\
	gphoto2-context.c	\
	exif.c exif.h		\
	gphoto2-file.c		\
	gphoto2-filesys.c	\
	gamma.c gamma.h		\
	jpeg.c jpeg.h		\
	gphoto2-list.c		\
	pattrec.c pattrec.h	\
	gphoto2-result.c	\
	gphoto2-setting.c	\
	gphoto2-widget.c
libgphoto2_la_LIBADD =					\
	$(top_builddir)/libexif/libexif.la		\
	$(top_builddir)/libgphoto2_port/libgphoto2_port/libgphoto2_port.la\
	$(LTDL_LIBS)\
	-lm $(INTLLIBS)

libgphoto2includedir = $(includedir)/gphoto2
libgphoto2include_HEADERS =	\
	gphoto2.h		\
	gphoto2-abilities-list.h\
	gphoto2-camera.h	\
	gphoto2-context.h	\
	gphoto2-file.h		\
	gphoto2-filesys.h	\
	gphoto2-library.h	\
	gphoto2-list.h		\
	gphoto2-result.h	\
	gphoto2-setting.h	\
	gphoto2-widget.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgphoto2.pc
EXTRA_DIST = libgphoto2.pc.in gphoto2-library.c

