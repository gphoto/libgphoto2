camlib_LTLIBRARIES = libgphoto2_ez200.la

INCLUDES =                              		\
        -I$(top_srcdir)/intl				\
        -I$(top_srcdir)/libgphoto2_port			\
	-I$(top_srcdir)/libgphoto2_port/libgphoto2_port	\
        -I$(top_srcdir)					\
	-I$(top_srcdir)/libgphoto2			\
	-I$(top_builddir)/libgphoto2

libgphoto2_ez200_la_SOURCES = library.c ez200.c ez200.h
libgphoto2_ez200_la_LDFLAGS = -module -avoid-version
libgphoto2_ez200_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
