camlib_LTLIBRARIES = libgphoto2_sq905.la

INCLUDES =                              		\
        -I$(top_srcdir)/intl				\
        -I$(top_srcdir)/libgphoto2_port			\
	-I$(top_srcdir)/libgphoto2_port/libgphoto2_port	\
        -I$(top_srcdir)					\
	-I$(top_srcdir)/libgphoto2			\
	-I$(top_builddir)/libgphoto2

libgphoto2_sq905_la_SOURCES = library.c sq905.c sq905.h postprocess.c 
libgphoto2_sq905_la_LDFLAGS = -module -avoid-version
libgphoto2_sq905_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
