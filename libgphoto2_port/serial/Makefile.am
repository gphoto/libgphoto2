## Specify some includes and versioning
INCLUDES = -I../ $(VERSION_FLAGS) -I$(top_srcdir)/include
VERSION_FLAGS = -DLIBGPPORT_VERSION=\"@LIBGPPORT_VERSION@\" -DHAVE_TERMIOS_H
CFLAGS = @CFLAGS@ @IOLIB_CFLAGS@ -g -I$(top_srcdir)/libgphoto2_port
LDFLAGS = @LDFLAGS@ -g 

## Compile the IO library into a shared library
iolibdir = $(prefix)/lib/gphoto2_port
iolib_LTLIBRARIES = libgphoto2_port_serial.la
libgphoto2_port_serial_la_LDFLAGS = -version-info @LIBGPPORT_VERSION_INFO@ -DHAVE_TERMIOS_H $(SERIAL_LDFLAGS) 
libgphoto2_port_serial_la_SOURCES = unix.c
