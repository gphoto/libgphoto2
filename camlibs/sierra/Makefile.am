EXTRA_DIST = sierra.df makefile.os2 PROTOCOL

camlibdir = $(libdir)/gphoto2/$(VERSION)
camlib_LTLIBRARIES = libgphoto2_sierra.la

INCLUDES =                                              \
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2			\
	$(EXIF_CFLAGS)

libgphoto2_sierra_la_SOURCES =\
 sierra.c sierra.h\
 library.c library.h\
 sierra-usbwrap.c sierra-usbwrap.h\
 sierra-desc.h sierra-desc.c nikon-desc.c olympus-desc.c

libgphoto2_sierra_la_LDFLAGS = -module -avoid-version
libgphoto2_sierra_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la $(EXIF_LIBS)
