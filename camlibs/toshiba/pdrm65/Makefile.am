camlib_LTLIBRARIES = libgphoto2_toshiba_pdrm65.la

INCLUDES =                                              \
        -I$(top_srcdir)/intl				\
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2			\
	-I$(top_builddir)/libgphoto2

libgphoto2_toshiba_pdrm65_la_SOURCES = pdrm65.c library.c pdrm65.h
libgphoto2_toshiba_pdrm65_la_LDFLAGS = -module -avoid-version
libgphoto2_toshiba_pdrm65_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
