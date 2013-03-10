lib_LTLIBRARIES = libgphoto2_port.la

libgphoto2_port_la_CPPFLAGS = 		\
	-I$(srcdir) -I$(top_srcdir)	\
	$(AM_CPPFLAGS) $(CPPFLAGS)	\
	$(LTDLINCL)					

# Notes about the list of exported symbols:
#    gp_* also contains internal symbols which are intended for
#         INTERNAL USE ONLY and should be converted into gpi_* symbols
#    gp_log_* is the gphoto2 logging API for use by
#             - libgphoto2_port and iolibs
#             - libgphoto2      and camlibs
#             - frontends (optional)
#    gp_system_* is a portable interface to a few filesystem functions
#             Used by
#             - libgphoto2_port and iolibs
#             - libgphoto2      and camlibs
#             - the gphoto2 frontend (NOT for other frontends)
#             Other frontends use their respective toolkit (glib, Qt, ...)
#    gp_port_* is the "real" gphoto2_port API for use by
#             - libgphoto2      and camlibs
#             - frontends
#
# Yes, this is messy, but we are going to clean this up.

libgphoto2_port_la_LDFLAGS = \
	-no-undefined \
	@VERSIONMAPLDFLAGS@ \
	-version-info @LIBGPHOTO2_PORT_VERSION_INFO@

libgphoto2_port_la_LIBADD = \
	$(LIBLTDL) 			\
	$(INTLLIBS)
# The libtool docs describe these params, but they don't build.
#	"-dlopen" self \
#	"-dlopen" $(top_builddir)/serial/....la \
#	"-dlopen" $(top_builddir)/usb/....la

libgphoto2_port_la_SOURCES =		\
	gphoto2-port-info-list.c	\
	gphoto2-port-info.h		\
	gphoto2-port-log.c		\
	gphoto2-port-version.c		\
	gphoto2-port.c 			\
	gphoto2-port-portability.c	\
	gphoto2-port-result.c

libgphoto2_port_la_DEPENDENCIES = \
	$(top_srcdir)/gphoto2/gphoto2-port-version.h		\
	$(top_srcdir)/gphoto2/gphoto2-port-library.h		\
	$(srcdir)/libgphoto2_port.ver

# Note: If you have problem with this file not being put into
#       the source tarball correctly at "make dist", this may
#       be the result of tar not creating archives with >99
#       characters of path length.
EXTRA_DIST = \
	libgphoto2_port.ver

../libltdl/libltdlc.la:
	cd ../libltdl && $(MAKE) libltdlc.la

