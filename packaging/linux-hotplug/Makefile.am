EXTRA_DIST = usbcam.console usbcam.group usbcam.user usbcam.x11-app

# FIXME: This should only be built if target system is linux - not if
#       the building system is linux.
if HAVE_LINUX

hotplugdoc_DATA = usbcam.console usbcam.group usbcam.user usbcam.x11-app

INCLUDES =	\
	-I$(top_srcdir)/libgphoto2	\
	-I$(top_srcdir)/libgphoto2_port/libgphoto2_port

# FIXME: utilsdir be modifiable by a ./configure option to adapt to local
#       policies
utilsdir = $(libdir)/$(PACKAGE)
utils_PROGRAMS = print-usb-usermap

print_usb_usermap_SOURCE = print-usb-usermap.c
print_usb_usermap_LDADD = ../../libgphoto2/libgphoto2.la

endif
