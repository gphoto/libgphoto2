camlib_LTLIBRARIES = libgphoto2_sipixblink.la

INCLUDES =						\
        -I$(top_srcdir)/intl				\
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port	\
        -I$(top_srcdir)/libgphoto2

libgphoto2_sipixblink_la_SOURCES = sipixblink.c
libgphoto2_sipixblink_la_LDFLAGS = -module -avoid-version
libgphoto2_sipixblink_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
