EXTRA_DIST = makefile.os2 usb.df
INCLUDES = -I$(top_srcdir) $(USB_CFLAGS)

## Compile the IO library into a shared library
iolibdir = $(prefix)/lib/gphoto2_port
iolib_LTLIBRARIES = libgphoto2_port_usb.la
libgphoto2_port_usb_la_LDFLAGS = -module -avoid-version $(USB_LIBS)
libgphoto2_port_usb_la_SOURCES = libusb.c
