camlibdir = $(libdir)/gphoto2/$(VERSION)
camlib_LTLIBRARIES = libgphoto2_mustek.la

INCLUDES = \
	-I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2



libgphoto2_mustek_la_SOURCES = mdc800.c core.c image.c io.c rs232.c usb.c core.h image.h io.h mdc800.h mdc800_spec.h print.h rs232.h usb.h

libgphoto2_mustek_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
libgphoto2_mustek_la_LDFLAGS = -module -avoid-version
