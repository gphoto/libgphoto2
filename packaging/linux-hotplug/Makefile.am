EXTRA_DIST = usbcam.console usbcam.group usbcam.user usbcam.x11-app

# FIXME: This should only be built if target system is linux - not if
#       the building system is linux.
if HAVE_LINUX

hotplugdoc_DATA = usbcam.console usbcam.group usbcam.user usbcam.x11-app

INCLUDES =	\
	-I$(top_srcdir)/libgphoto2	\
	-I$(top_srcdir)/libgphoto2_port/libgphoto2_port

# FIXME: libexec is a BSDism, but the libexecdir should nevertheless
#       be modifiable by a ./configure option
libexecdir = $(libdir)/$(PACKAGE)
libexec_PROGRAMS = print-usb-usermap

print_usb_usermap_SOURCE = print-usb-usermap.c
print_usb_usermap_LDADD = ../../libgphoto2/libgphoto2.la

endif
