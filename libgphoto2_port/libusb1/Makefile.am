iolib_LTLIBRARIES = usb1.la

usb1_la_LDFLAGS = -module -no-undefined -avoid-version \
	-export-dynamic \
        -export-symbols $(top_srcdir)/iolib.sym
usb1_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(CPPFLAGS) \
	$(INTL_CFLAGS) \
	$(LIBUSB1_CFLAGS)

usb1_la_DEPENDENCIES = $(top_srcdir)/iolib.sym
usb1_la_LIBADD = \
	$(top_builddir)/libgphoto2_port/libgphoto2_port.la \
	$(LIBUSB1_LIBS)			\
	$(INTLLIBS)
usb1_la_SOURCES = libusb1.c
