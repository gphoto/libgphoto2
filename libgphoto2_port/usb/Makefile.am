EXTRA_DIST = makefile.os2 usb.df usb-os2.c

iolib_LTLIBRARIES = libgphoto2_port_usb.la

libgphoto2_port_usb_la_CPPFLAGS = \
	$(AM_CPPFLAGS) $(AM_CFLAGS)	\
	$(LIBUSB_CFLAGS)

libgphoto2_port_usb_la_LDFLAGS = -module -no-undefined -avoid-version
libgphoto2_port_usb_la_LIBADD = \
	$(LIBUSB_LIBS)			\
	$(top_builddir)/libgphoto2_port/libgphoto2_port.la
libgphoto2_port_usb_la_SOURCES = libusb.c
