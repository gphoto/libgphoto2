## Specify some includes and versioning
INCLUDES = -I../ $(VERSION_FLAGS) -I$(top_srcdir)/include
VERSION_FLAGS = -DLIBGPPORT_VERSION=\"@LIBGPPORT_VERSION@\"
CFLAGS = @CFLAGS@ @IOLIB_CFLAGS@ -g -I$(top_srcdir)/libgphoto2_port
LDFLAGS = @LDFLAGS@ -g 

## Compile the IO library into a shared library
iolibdir = $(prefix)/lib/gphoto2_port
iolib_LTLIBRARIES = libgphoto2_port_parallel.la
libgphoto2_port_parallel_la_LDFLAGS = -version-info @LIBGPPORT_VERSION_INFO@ -DHAVE_TERMIOS_H 
libgphoto2_port_parallel_la_SOURCES = unix.c
