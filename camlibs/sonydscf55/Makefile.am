camlibdir = $(libdir)/gphoto2/$(VERSION)
camlib_LTLIBRARIES = libgphoto2_sony_dscf55.la

INCLUDES =                                              \
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2

libgphoto2_sony_dscf55_la_SOURCES = sony.c sony.h camera.c nls.h
libgphoto2_sony_dscf55_la_LDFLAGS = -module -avoid-version
libgphoto2_sony_dscf55_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
