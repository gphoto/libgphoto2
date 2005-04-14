EXTRA_DIST = ptp-pack.c

camlib_LTLIBRARIES = libgphoto2_ptp2.la

libgphoto2_ptp2_la_SOURCES = ptp.c ptp.h library.c ptp-bugs.h
libgphoto2_ptp2_la_LDFLAGS = -module -avoid-version
libgphoto2_ptp2_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
