EXTRA_DIST = makefile.os2 serial.df

## Compile the IO library into a libtool module shared library
iolib_LTLIBRARIES = serial.la

serial_la_LDFLAGS = -module -no-undefined -avoid-version \
	-export-dynamic \
        -export-symbols $(top_srcdir)/iolib.sym
serial_la_CPPFLAGS = -I$(top_srcdir)/libgphoto2_port $(INTL_CFLAGS)
serial_la_LIBADD =				\
	$(top_builddir)/libgphoto2_port/libgphoto2_port.la	\
	$(SERIAL_LIBS)
serial_la_SOURCES = unix.c
