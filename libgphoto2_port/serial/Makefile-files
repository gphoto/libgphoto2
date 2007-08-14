## Compile the IO library into a libtool module shared library
iolib_LTLIBRARIES = serial.la

serial_la_LDFLAGS = -module -no-undefined -avoid-version \
	-export-dynamic \
        -export-symbols $(top_srcdir)/iolib.sym
serial_la_CPPFLAGS = $(AM_CPPFLAGS) $(INTL_CFLAGS) $(CPPFLAGS)
serial_la_DEPENDENCIES = $(top_srcdir)/iolib.sym
serial_la_LIBADD =				\
	$(top_builddir)/libgphoto2_port/libgphoto2_port.la	\
	$(SERIAL_LIBS)						\
	$(INTLLIBS)
serial_la_SOURCES = unix.c
