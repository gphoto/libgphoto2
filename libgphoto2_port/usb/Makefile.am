EXTRA_DIST = usb-os2.c

iolib_LTLIBRARIES = usb.la

usb_la_LDFLAGS = -module -no-undefined -avoid-version \
	-export-dynamic \
        -export-symbols $(top_srcdir)/iolib.sym
usb_la_CPPFLAGS = \
	$(INTL_CFLAGS) \
	$(AM_CPPFLAGS) $(LIBUSB_CFLAGS) \
	$(CPPFLAGS)
usb_la_DEPENDENCIES = $(top_srcdir)/iolib.sym
usb_la_LIBADD = \
	$(LIBUSB_LIBS)			\
	$(INTLLIBS)			\
	$(top_builddir)/libgphoto2_port/libgphoto2_port.la
usb_la_SOURCES = libusb.c
