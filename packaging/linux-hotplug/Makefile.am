EXTRA_DIST = usbcam.console usbcam.group usbcam.user

# no need to build or install the following on non-linux systems
if HAVE_LINUX

docdir = @DOC_DIR@/linux-hotplug
doc_DATA = usbcam.console usbcam.group usbcam.user

INCLUDES =	\
	-I$(top_srcdir)/libgphoto2	\
	-I$(top_srcdir)/libgphoto2_port/libgphoto2_port

noinst_PROGRAMS = print-usb-usermap

# FIXME: This should only be built if target system is linux
print_usb_usermap_SOURCE = print-usb-usermap.c
print_usb_usermap_LDADD = ../../libgphoto2/libgphoto2.la

endif
