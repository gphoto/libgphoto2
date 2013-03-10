lib_LTLIBRARIES = libgphoto2.la

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

libgphoto2_la_CPPFLAGS =				\
	-I$(top_srcdir) -I$(top_builddir)		\
	-I$(top_srcdir)/libgphoto2_port -I$(top_builddir)/libgphoto2_port		\
	$(AM_CPPFLAGS) $(CPPFLAGS)			\
	$(LTDLINCL)					\
	$(LIBEXIF_CFLAGS)

libgphoto2_la_LDFLAGS =	\
	-no-undefined	\
	-export-symbols $(srcdir)/libgphoto2.sym \
	-version-info @LIBGPHOTO2_VERSION_INFO@

libgphoto2_la_SOURCES = 	\
	gphoto2-abilities-list.c\
	ahd_bayer.c 		\
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
	$(top_srcdir)/gphoto2/gphoto2-version.h \
	$(srcdir)/libgphoto2.sym

EXTRA_DIST = gphoto2-library.c libgphoto2.sym

DISTCLEANFILES = _stdint.h
