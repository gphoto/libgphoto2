EXTRA_DIST = makefile.os2 usb.df
INCLUDES = -I$(top_srcdir)/libgphoto2_port $(USB_CFLAGS)

## Compile the IO library into a shared library
iolibdir = $(prefix)/lib/gphoto2_port/$(VERSION)
iolib_LTLIBRARIES = libgphoto2_port_usb.la
libgphoto2_port_usb_la_LDFLAGS = -module -avoid-version
libgphoto2_port_usb_la_LIBADD = $(USB_LIBS)
libgphoto2_port_usb_la_SOURCES = libusb.c
