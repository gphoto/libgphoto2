camlib_LTLIBRARIES = libgphoto2_aox.la

INCLUDES =                              		\
        -I$(top_srcdir)/intl				\
        -I$(top_srcdir)/libgphoto2_port			\
	-I$(top_srcdir)/libgphoto2_port/libgphoto2_port	\
        -I$(top_srcdir)					\
	-I$(top_srcdir)/libgphoto2			\
	-I$(top_builddir)/libgphoto2

libgphoto2_aox_la_SOURCES = library.c aox.c aox.h
libgphoto2_aox_la_LDFLAGS = -module -avoid-version
libgphoto2_aox_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
