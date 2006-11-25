EXTRA_DIST = usbcam.console usbcam.group usbcam.user usbcam.x11-app \
	90-gphoto-camera-policy.fdi gphoto-set-procperm

# FIXME: This should only be built if target system is linux - not if
#       the building system is linux.
if HAVE_LINUX

hotplugdoc_DATA = usbcam.console usbcam.group usbcam.user usbcam.x11-app


utils_PROGRAMS = print-usb-usermap

print_usb_usermap_SOURCE = print-usb-usermap.c
print_usb_usermap_LDADD = \
	$(top_builddir)/libgphoto2/libgphoto2.la \
	$(top_builddir)/libgphoto2_port/libgphoto2_port/libgphoto2_port.la \
	$(LIBEXIF_LIBS)
print_usb_usermap_CPPFLAGS = \
	$(AM_CPPFLAGS) $(CPPFLAGS) \
	-I$(top_srcdir)/libgphoto2 \
	-I$(top_srcdir)/libgphoto2_port/libgphoto2_port

endif
