camlibdir = $(libdir)/gphoto2/$(VERSION)
camlib_LTLIBRARIES = libgphoto2_pccam300.la

INCLUDES =                                              \
        -I$(top_srcdir)/intl				\
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2

libgphoto2_pccam300_la_SOURCES = pccam300.c pccam300.h library.c 
libgphoto2_pccam300_la_LDFLAGS = -module -avoid-version
libgphoto2_pccam300_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
