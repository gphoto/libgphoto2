camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_samsung.la

INCLUDES =                                              \
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2

libgphoto2_samsung_la_SOURCES = samsung.c

libgphoto2_samsung_la_LDFLAGS =\
 -module -avoid-version\
 -L$(shell pwd)/$(top_builddir)/libgphoto2/@dotlibs@

libgphoto2_samsung_la_LIBADD = -lgphoto2
