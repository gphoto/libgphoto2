EXTRA_DIST = README ptp-pack.c test_ptp.c

camlibdir = $(libdir)/gphoto2/$(VERSION)
camlib_LTLIBRARIES = libgphoto2_ptp.la

INCLUDES =                                              \
        -I$(top_srcdir)/intl				\
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2

libgphoto2_ptp_la_SOURCES = ptp.c ptp.h library.c
libgphoto2_ptp_la_LDFLAGS = -module -avoid-version
libgphoto2_ptp_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
