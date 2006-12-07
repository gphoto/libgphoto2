EXTRA_DIST = usb-os2.c

iolib_LTLIBRARIES = usb.la

usb_la_LDFLAGS = -module -no-undefined -avoid-version \
	-export-dynamic \
        -export-symbols $(top_srcdir)/iolib.sym
usb_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(CPPFLAGS) \
	$(INTL_CFLAGS) \
	$(LIBUSB_CFLAGS)

usb_la_DEPENDENCIES = $(top_srcdir)/iolib.sym
usb_la_LIBADD = \
	$(top_builddir)/libgphoto2_port/libgphoto2_port.la \
	$(LIBUSB_LIBS)			\
	$(INTLLIBS)
usb_la_SOURCES = libusb.c
