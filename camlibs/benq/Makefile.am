camlib_LTLIBRARIES = libgphoto2_benq.la

INCLUDES =                                              \
        -I$(top_srcdir)/intl				\
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2			\
	-I$(top_builddir)/libgphoto2

libgphoto2_benq_la_SOURCES = benq.c \
				      benq.h library.c
libgphoto2_benq_la_LDFLAGS = -module -avoid-version
libgphoto2_benq_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
