EXTRA_DIST = makefile.os2 serial.df

INCLUDES = -I$(top_srcdir)

## Compile the IO library into a shared library
iolibdir = $(prefix)/lib/gphoto2_port
iolib_LTLIBRARIES = libgphoto2_port_serial.la
libgphoto2_port_serial_la_LDFLAGS = -module -avoid-version $(SERIAL_LDFLAGS) 
libgphoto2_port_serial_la_SOURCES = unix.c
