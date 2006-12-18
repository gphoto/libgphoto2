utils_PROGRAMS = print-camera-list
udevscript_SCRIPTS = check-ptp-camera
EXTRA_DIST = check-ptp-camera

print_camera_list_SOURCE = print-camera-list.c
print_camera_list_LDADD = \
	$(top_builddir)/libgphoto2/libgphoto2.la \
	$(top_builddir)/libgphoto2_port/libgphoto2_port/libgphoto2_port.la \
	$(LIBEXIF_LIBS)
