camlib_LTLIBRARIES = libgphoto2_toshiba_pdrm11.la

INCLUDES =                                              \
        -I$(top_srcdir)/intl				\
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2			\
	-I$(top_builddir)/libgphoto2

libgphoto2_toshiba_pdrm11_la_SOURCES = library.c pdrm11.c pdrm11.h
libgphoto2_toshiba_pdrm11_la_LDFLAGS = -module -avoid-version
libgphoto2_toshiba_pdrm11_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
