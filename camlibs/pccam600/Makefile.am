camlibdir = $(libdir)/gphoto2/$(VERSION)
camlib_LTLIBRARIES = libgphoto2_pccam600.la

INCLUDES =                                              \
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2

libgphoto2_pccam600_la_SOURCES = pccam600.c pccam600.h library.c 
libgphoto2_pccam600_la_LDFLAGS = -module -avoid-version
libgphoto2_pccam600_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
