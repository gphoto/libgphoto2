camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_digita.la

INCLUDES = -I$(top_srcdir)/include

libgphoto2_digita_la_SOURCES = digita.c serial.c usb.c commands.c digita.h

libgphoto2_digita_la_LDFLAGS =\
 -avoid-version\
 -L$(shell pwd)/$(top_builddir)/libgphoto2/.libs

libgphoto2_digita_la_LIBADD = -lgphoto2
