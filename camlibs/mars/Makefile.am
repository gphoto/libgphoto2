camlib_LTLIBRARIES = libgphoto2_mars.la

INCLUDES =                              		\
        -I$(top_srcdir)/intl				\
        -I$(top_srcdir)/libgphoto2_port			\
	-I$(top_srcdir)/libgphoto2_port/libgphoto2_port	\
        -I$(top_srcdir)					\
	-I$(top_srcdir)/libgphoto2			\
	-I$(top_builddir)/libgphoto2

libgphoto2_mars_la_SOURCES = library.c mars.c mars.h
libgphoto2_mars_la_LDFLAGS = -module -avoid-version
libgphoto2_mars_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
