camlibdir = $(libdir)/gphoto2/$(VERSION)
camlib_LTLIBRARIES = libgphoto2_sipix.la

EXTRA_DIST = web2.html

INCLUDES = \
	-I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2



libgphoto2_sipix_la_SOURCES = sipix.c web2.c web2.h

libgphoto2_sipix_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
libgphoto2_sipix_la_LDFLAGS = -module -avoid-version
