camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_agfa_cl18.la

INCLUDES = -I$(top_srcdir)/include

libgphoto2_agfa_cl18_la_SOURCES = agfa.c commands.c usb.c agfa.h

libgphoto2_agfa_cl18_la_LDFLAGS =\
 -avoid-version\
 -L$(shell pwd)/$(top_builddir)/libgphoto2/.libs

libgphoto2_agfa_cl18_la_LIBADD = -lgphoto2
