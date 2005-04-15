lib_LTLIBRARIES = libgphoto2_port.la

libgphoto2_port_la_CPPFLAGS = \
	$(LTDLINCL)					\
	$(AM_CPPFLAGS) $(CPPFLAGS)

# FIXME: -export-symbols $(srcdir)/libgphoto2_port.sym
libgphoto2_port_la_LDFLAGS = \
	-no-undefined \
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
	gphoto2-port-log.c		\
	gphoto2-port-version.c		\
	gphoto2-port-version.h		\
	gphoto2-port-library.h		\
	gphoto2-port.c 			\
	gphoto2-port-portability.c	\
	gphoto2-port-result.c

libgphoto2_port_la_DEPENDENCIES = \
	$(LIBLTDL)

libgphoto2_portincludedir = $(includedir)/gphoto2
libgphoto2_portinclude_HEADERS =	\
	gphoto2-port.h			\
	gphoto2-port-info-list.h	\
	gphoto2-port-log.h		\
	gphoto2-port-version.h		\
	gphoto2-port-portability.h	\
	gphoto2-port-result.h

# Note: If you have problem with this file not being put into
#       the source tarball correctly at "make dist", this may
#       be the result of tar not creating archives with >99
#       characters of path length.
EXTRA_DIST = gphoto2-port-portability-os2.h
