camlib_LTLIBRARIES = libgphoto2_gsmart300.la

INCLUDES =                                              \
        -I$(top_srcdir)/intl				\
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2			\
	-I$(top_builddir)/libgphoto2

libgphoto2_gsmart300_la_SOURCES = gsmart300.c gsmart300.h library.c \
                               gsmart300-jpeg-header.h 
libgphoto2_gsmart300_la_LDFLAGS = -module -avoid-version
libgphoto2_gsmart300_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
