## Specify some includes and versioning
INCLUDES = -I../ $(VERSION_FLAGS) -I$(top_srcdir)/include
VERSION_FLAGS = -DLIBGPPORT_VERSION=\"@LIBGPPORT_VERSION@\"
CFLAGS = @CFLAGS@ @IOLIB_CFLAGS@ \
	-DIOLIBS=\"$(libdir)/gphoto2_port\" \
	-g
LDFLAGS = @LDFLAGS@ -g -ldl

## Compile the IO library into a shared library
lib_LTLIBRARIES = libgphoto2_port.la
libgphoto2_port_la_LDFLAGS = -version-info @LIBGPPORT_VERSION_INFO@
libgphoto2_port_la_SOURCES =	\
	gphoto2-port-core.c	\
	gphoto2-port-log.c	\
	gp_port.c \
	result.c \
	portability.c

libgphoto2_portincludedir = $(includedir)/gphoto2
libgphoto2_portinclude_HEADERS =	\
	gphoto2-port-core.h		\
	gphoto2-port-log.h
