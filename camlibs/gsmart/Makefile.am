camlibdir = $(libdir)/gphoto2/$(VERSION)
camlib_LTLIBRARIES = libgphoto2_gsmart.la

INCLUDES =                                              \
        -I$(top_srcdir)/intl				\
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2			\
	-I$(top_builddir)/libgphoto2

libgphoto2_gsmart_la_SOURCES = gsmart.c gsmart.h library.c \
                               gsmart-jpeg-header.h gsmart-registers.h
libgphoto2_gsmart_la_LDFLAGS = -module -avoid-version
libgphoto2_gsmart_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
