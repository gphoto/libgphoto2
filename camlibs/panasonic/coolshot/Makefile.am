camlibdir = $(libdir)/gphoto2/$(VERSION)
camlib_LTLIBRARIES = libgphoto2_panasonic_coolshot.la

INCLUDES =                                              \
        -I$(top_srcdir)/intl				\
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2

libgphoto2_panasonic_coolshot_la_SOURCES = coolshot.c library.c library.h
libgphoto2_panasonic_coolshot_la_LDFLAGS = -module -avoid-version
libgphoto2_panasonic_coolshot_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
