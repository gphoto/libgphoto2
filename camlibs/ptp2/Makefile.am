EXTRA_DIST = ptp-pack.c

camlib_LTLIBRARIES = libgphoto2_ptp2.la

INCLUDES =                                              \
        -I$(top_srcdir)/intl				\
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2			\
	-I$(top_builddir)/libgphoto2

libgphoto2_ptp2_la_SOURCES = ptp.c ptp.h library.c
libgphoto2_ptp2_la_LDFLAGS = -module -avoid-version
libgphoto2_ptp2_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la

noinst_PROGRAMS = ptpcam
ptpcam_SOURCES = test_ptp.c
ptpcam_LDADD = -lusb ptp.o
