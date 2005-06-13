EXTRA_DIST = 

iolib_LTLIBRARIES = disk.la

disk_la_LDFLAGS = -module -no-undefined -avoid-version \
	-export-dynamic \
        -export-symbols $(top_srcdir)/iolib.sym
disk_la_CPPFLAGS = \
	-I$(top_srcdir)/libgphoto2_port $(INTL_CFLAGS) \
	$(AM_CPPFLAGS) $(CPPFLAGS) $(LIBUSB_CFLAGS) $(HAL_CFLAGS)
disk_la_LIBADD = \
	$(top_builddir)/libgphoto2_port/libgphoto2_port.la \
	$(HAL_LIBS)
disk_la_SOURCES = disk.c
