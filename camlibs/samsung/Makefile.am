camlibdir = $(libdir)/gphoto2/$(VERSION)
camlib_LTLIBRARIES = libgphoto2_samsung.la

INCLUDES =                                              \
        -I$(top_srcdir)/intl				\
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2

libgphoto2_samsung_la_SOURCES = samsung.c
libgphoto2_samsung_la_LDFLAGS = -module -avoid-version
libgphoto2_samsung_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
