EXTRA_DIST = digita.df makefile.os2

camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_digita.la

INCLUDES =                                              \
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2

libgphoto2_digita_la_SOURCES = digita.c serial.c usb.c commands.c digita.h
libgphoto2_digita_la_LDFLAGS = -module -avoid-version
libgphoto2_digita_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
