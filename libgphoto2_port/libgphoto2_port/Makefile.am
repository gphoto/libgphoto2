INCLUDES = 					\
	-I$(top_srcdir)				\
	-DIOLIBS=\"$(libdir)/gphoto2_port\"

## What is this for?
CFLAGS = @CFLAGS@ -g
LDFLAGS = @LDFLAGS@ -g

## Compile the IO library into a shared library
lib_LTLIBRARIES = libgphoto2_port.la
libgphoto2_port_la_LDFLAGS = -version-info @LIBGPPORT_VERSION_INFO@
libgphoto2_port_la_LIBADD = -ldl $(INTLLIBS)
libgphoto2_port_la_SOURCES =		\
	gphoto2-port-info-list.c	\
	gphoto2-port-log.c		\
	gphoto2-port-library.h		\
	gphoto2-port.c 			\
	gphoto2-port-portability.c	\
	gphoto2-port-result.c

libgphoto2_portincludedir = $(includedir)/gphoto2
libgphoto2_portinclude_HEADERS =	\
	gphoto2-port.h			\
	gphoto2-port-info-list.h	\
	gphoto2-port-log.h		\
	gphoto2-port-portability.h	\
	gphoto2-port-result.h

EXTRA_DIST = gphoto2-port-portability-os2.h
