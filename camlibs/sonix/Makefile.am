camlib_LTLIBRARIES = libgphoto2_sonix.la

INCLUDES =                              		\
        -I$(top_srcdir)/intl				\
        -I$(top_srcdir)/libgphoto2_port			\
	-I$(top_srcdir)/libgphoto2_port/libgphoto2_port	\
        -I$(top_srcdir)					\
	-I$(top_srcdir)/libgphoto2			\
	-I$(top_builddir)/libgphoto2

libgphoto2_sonix_la_SOURCES = library.c sonix.c sonix.h
libgphoto2_sonix_la_LDFLAGS = -module -avoid-version
libgphoto2_sonix_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
