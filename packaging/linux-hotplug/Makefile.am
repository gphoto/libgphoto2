EXTRA_DIST = usbcam.console usbcam.group usbcam.user

docdir = @DOC_DIR@/linux-hotplug
doc_DATA = usbcam.console usbcam.group usbcam.user

INCLUDES =	\
	-I$(top_srcdir)/libgphoto2	\
	-I$(top_srcdir)/libgphoto2_port/libgphoto2_port

noinst_PROGRAMS = print-usb-usermap

print_usb_usermap_SOURCE = print-usb-usermap.c
print_usb_usermap_LDADD = $(top_srcdir)/libgphoto2/libgphoto2.la
