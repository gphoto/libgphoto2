EXTRA_DIST = makefile.os2 serial.df

INCLUDES = -I$(top_srcdir)/libgphoto2_port

## Compile the IO library into a shared library
iolibdir = $(libdir)/gphoto2_port/$(VERSION)
iolib_LTLIBRARIES = libgphoto2_port_serial.la
libgphoto2_port_serial_la_LDFLAGS = -module -avoid-version $(SERIAL_LDFLAGS) 
libgphoto2_port_serial_la_SOURCES = unix.c
