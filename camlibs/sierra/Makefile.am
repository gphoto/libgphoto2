EXTRA_DIST = sierra.df makefile.os2 PROTOCOL

camlib_LTLIBRARIES = sierra.la

sierra_la_CPPFLAGS = $(CPPFLAGS) $(AM_CPPFLAGS) $(LIBEXIF_CFLAGS)
sierra_la_SOURCES =\
	sierra.c sierra.h \
	library.c library.h \
	sierra-usbwrap.c sierra-usbwrap.h \
	sierra-desc.h sierra-desc.c \
	nikon-desc.c olympus-desc.c epson-desc.c
sierra_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
sierra_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la $(LIBEXIF_LIBS)
