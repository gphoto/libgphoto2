utils_PROGRAMS = print-camera-list

# print_camera_list_CPPFLAGS = -I$(top_srcdir) -I$(top_builddir)
print_camera_list_SOURCE = print-camera-list.c
print_camera_list_LDADD = \
	$(top_builddir)/libgphoto2/libgphoto2.la \
	$(top_builddir)/libgphoto2_port/libgphoto2_port/libgphoto2_port.la \
	$(LIBEXIF_LIBS)
