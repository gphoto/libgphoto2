## Specify some includes and versioning
INCLUDES = -I../ $(VERSION_FLAGS) -I$(top_srcdir)/include
VERSION_FLAGS = -DLIBGPPORT_VERSION=\"@LIBGPPORT_VERSION@\"
CFLAGS = @CFLAGS@ @IOLIB_CFLAGS@ -g `libusb-config --cflags`
LDFLAGS = @LDFLAGS@ -g `libusb-config --libs`

## Compile the IO library into a shared library
iolibdir = $(prefix)/lib/gphoto2_port
iolib_LTLIBRARIES = libgphoto2_port_usb.la
libgphoto2_port_usb_la_LDFLAGS = -version-info @LIBGPPORT_VERSION_INFO@
libgphoto2_port_usb_la_SOURCES = libusb.c
