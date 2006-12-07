EXTRA_DIST = 

iolib_LTLIBRARIES = ptpip.la

ptpip_la_LDFLAGS = -module -no-undefined -avoid-version \
	-export-dynamic \
        -export-symbols $(top_srcdir)/iolib.sym
ptpip_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(INTL_CFLAGS) \
	$(CPPFLAGS) 
ptpip_la_DEPENDENCIES = $(top_srcdir)/iolib.sym
ptpip_la_LIBADD = $(top_builddir)/libgphoto2_port/libgphoto2_port.la @MDNS_LIBS@
ptpip_la_SOURCES = ptpip.c
