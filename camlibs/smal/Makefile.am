camlibdir = $(libdir)/gphoto2/$(VERSION)
camlib_LTLIBRARIES = libgphoto2_smal.la

INCLUDES =                                              \
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2

libgphoto2_smal_la_SOURCES = smal.c ultrapocket.c smal.h ultrapocket.h
libgphoto2_smal_la_LDFLAGS = -module -avoid-version
libgphoto2_smal_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
