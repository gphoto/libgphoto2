camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_ptp.la

INCLUDES = -I$(top_srcdir)/libgphoto2

libgphoto2_ptp_la_SOURCES = ptp.c ptp.h library.c

libgphoto2_ptp_la_LDFLAGS =\
 -module -avoid-version\
 -L$(shell pwd)/$(top_builddir)/libgphoto2/@dotlibs@

libgphoto2_ptp_la_LIBADD = -lgphoto2
