camlibdir = $(libdir)/gphoto2/$(VERSION)
camlib_LTLIBRARIES = libgphoto2_sx330z.la

INCLUDES =                                              \
        -I$(top_srcdir)/intl				\
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2			\
	-I$(top_builddir)/libgphoto2

libgphoto2_sx330z_la_SOURCES = library.c sx330z.c sx330z.h
libgphoto2_sx330z_la_LDFLAGS = -module -avoid-version
libgphoto2_sx330z_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
