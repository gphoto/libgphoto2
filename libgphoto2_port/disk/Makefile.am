EXTRA_DIST = 

iolib_LTLIBRARIES = disk.la

disk_la_LDFLAGS = -module -no-undefined -avoid-version \
	-export-dynamic \
        -export-symbols $(top_srcdir)/iolib.sym
disk_la_CPPFLAGS = \
	$(INTL_CFLAGS) \
	$(AM_CPPFLAGS) \
	$(LIBUSB_CFLAGS) $(HAL_CFLAGS) \
	$(CPPFLAGS)
disk_la_DEPENDENCIES = $(top_srcdir)/iolib.sym
disk_la_LIBADD = \
	$(top_builddir)/libgphoto2_port/libgphoto2_port.la \
	$(INTLLIBS) \
	$(HAL_LIBS)
disk_la_SOURCES = disk.c
