lib_LTLIBRARIES = libgphoto2.la

libgphoto2_la_CPPFLAGS =				\
	$(LTDLINCL)					\
	$(AM_CPPFLAGS) $(CPPFLAGS)			\
	$(LIBEXIF_CFLAGS)

# Notes about the list of exported symbols:
#    gp_* also contains internal symbols which are intended for
#         INTERNAL USE ONLY and should be converted into gpi_* symbols
#    gpi_* are for libgphoto2/camlib internal use only
#          NEVER USE gpi_* in frontends!
#    gp_* gphoto2 API, used in 
#         - frontends
#           BE AWARE that NOT ALL gp_* FUNCTIONS ARE CONSIDERED PUBLIC.
#
# Yes, this is messy, but we are going to clean this up.

libgphoto2_la_LDFLAGS =	\
	-no-undefined	\
	-export-symbols $(srcdir)/libgphoto2.sym \
	-version-info @LIBGPHOTO2_VERSION_INFO@

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
	gphoto2-result.c	\
	gphoto2-version.c	\
	gphoto2-version.h	\
	gphoto2-setting.c	\
	gphoto2-widget.c

libgphoto2_la_LIBADD =					\
	$(top_builddir)/libgphoto2_port/libgphoto2_port/libgphoto2_port.la \
	$(LIBLTDL)					\
	$(LIBEXIF_LIBS)					\
	-lm $(INTLLIBS)
# The libtool docs describe these params, but they don't build.
#	"-dlopen" self \
#	"-dlopen" $(top_builddir)/camlibs/canon/....la \
#	"-dlopen" $(top_builddir)/camlibs/ptp2/....la

libgphoto2_la_DEPENDENCIES = \
	$(LIBLTDL)


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
	gphoto2-version.h	\
	gphoto2-widget.h

pkgconfig_DATA = libgphoto2.pc

EXTRA_DIST = libgphoto2.pc.in gphoto2-library.c libgphoto2.sym

DISTCLEANFILES = _stdint.h gphoto2-endian.h

