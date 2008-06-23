utils_PROGRAMS = print-camera-list
if HAVE_UDEV
udevscript_SCRIPTS = check-ptp-camera
endif
EXTRA_DIST = check-ptp-camera

print_camera_list_SOURCE = print-camera-list.c
print_camera_list_LDADD = \
	$(top_builddir)/libgphoto2/libgphoto2.la \
	$(top_builddir)/libgphoto2_port/libgphoto2_port/libgphoto2_port.la \
	$(LIBLTDL) \
	$(LIBEXIF_LIBS)
