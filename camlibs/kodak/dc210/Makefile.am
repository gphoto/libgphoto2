EXTRA_DIST = kodak.df makefile.os2

camlibdir = $(libdir)/gphoto2/$(VERSION)
camlib_LTLIBRARIES = libgphoto2_kodak_dc210.la

INCLUDES =                                              \
        -I$(top_srcdir)/intl				\
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2

libgphoto2_kodak_dc210_la_SOURCES = dc210.c dc210.h library.c library.h
libgphoto2_kodak_dc210_la_LDFLAGS = -module -avoid-version
libgphoto2_kodak_dc210_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
