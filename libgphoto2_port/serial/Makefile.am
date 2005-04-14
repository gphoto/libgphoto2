EXTRA_DIST = makefile.os2 serial.df

INCLUDES = -I$(top_srcdir)/libgphoto2_port -I$(top_srcdir)/intl

## Compile the IO library into a shared library
iolib_LTLIBRARIES = libgphoto2_port_serial.la

libgphoto2_port_serial_la_LDFLAGS = -module -no-undefined -avoid-version
libgphoto2_port_serial_la_LIBADD =				\
	$(top_builddir)/libgphoto2_port/libgphoto2_port.la	\
	$(SERIAL_LIBS)
libgphoto2_port_serial_la_SOURCES = unix.c
