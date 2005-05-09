EXTRA_DIST = makefile.os2 usb.df usb-os2.c

iolib_LTLIBRARIES = usb.la

usb_la_LDFLAGS = -module -no-undefined -avoid-version \
	-export-dynamic \
        -export-symbols $(top_srcdir)/iolib.sym
usb_la_CPPFLAGS = \
	-I$(top_srcdir)/libgphoto2_port $(INTL_CFLAGS) \
	$(AM_CPPFLAGS) $(CPPFLAGS) $(LIBUSB_CFLAGS)
usb_la_LIBADD = \
	$(LIBUSB_LIBS)			\
	$(top_builddir)/libgphoto2_port/libgphoto2_port.la
usb_la_SOURCES = libusb.c
