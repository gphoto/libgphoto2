lib_LTLIBRARIES = libgphoto2.la

CFLAGS= @CFLAGS@ \
	-g \
	-Wall \
	-I$(top_srcdir)/include \
	-DCAMLIBS=\"$(prefix)/lib/gphoto2\" \
	-DDOCDIR=\"$(prefix)/doc/gphoto2\" \
	`gpio-config --cflags`

LDFLAGS = @LDFLAGS@ -g `gpio-config --libs`

libgphoto2_la_LDFLAGS = -version-info 0:0:0
libgphoto2_la_SOURCES = \
			core.c core.h \
			exif.c exif.h \
			file.c file.h \
			globals.h \
			library.c library.h \
			settings.c settings.h \
			util.c util.h \
			widget.c widget.h

confexecdir           = $(libdir)
confexec_DATA         = libgphoto2Conf.sh
EXTRA_DIST            = libgphoto2Conf.sh.in
CLEANFILES            = libgphoto2Conf.sh

libgphoto2Conf.sh: libgphoto2Conf.sh.in Makefile
## Use sed and then mv to avoid problems if the user interrupts.
	sed -e 's?\@LIBGPHOTO2_LIBDIR\@?$(LIBGPHOTO2_LIBDIR)?g' \
	    -e 's?\@LIBGPHOTO2_INCLUDEDIR\@?$(LIBGPHOTO2_INCLUDEDIR)?g' \
	    -e 's?\@LIBGPHOTO2_LIBS\@?$(LIBGPHOTO2_LIBS)?g' \
	    -e 's?\@LIBGPHOTO2_VERSION\@?$(LIBGPHOTO2_VERSION)?g' \
	      < $(srcdir)/libgphoto2Conf.sh.in > libgphoto2Conf.tmp \
	  && mv libgphoto2Conf.tmp libgphoto2Conf.sh
