EXTRA_DIST = kodak.df makefile.os2

camlib_LTLIBRARIES = libgphoto2_kodak_dc3200.la

INCLUDES =                                              \
        -I$(top_srcdir)/intl				\
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2

libgphoto2_kodak_dc3200_la_SOURCES = dc3200.c dc3200.h library.c library.h
libgphoto2_kodak_dc3200_la_LDFLAGS = -module -avoid-version
libgphoto2_kodak_dc3200_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
