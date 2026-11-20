utils_PROGRAMS = print-camera-list
if HAVE_UDEV
udevscript_SCRIPTS = check-ptp-camera
endif
EXTRA_DIST = check-ptp-camera

AM_CPPFLAGS += -I$(top_srcdir) -I$(top_builddir)  -I$(top_srcdir)/libgphoto2_port -I$(top_srcdir)/libgphoto2 -I$(top_builddir)/libgphoto2

print_camera_list_SOURCES = print-camera-list.c
print_camera_list_LDADD = \
	$(libgphoto2_la) \
	$(libgphoto2_port_la) \
	$(LIBLTDL) \
	$(LIBEXIF_LIBS)
