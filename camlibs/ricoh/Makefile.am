camlibdir = $(libdir)/gphoto2/$(VERSION)
camlib_LTLIBRARIES = libgphoto2_ricoh.la

INCLUDES =                                              \
        -I$(top_srcdir)/intl				\
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2

libgphoto2_ricoh_la_SOURCES = library.c ricoh.c ricoh.h crctab.h
libgphoto2_ricoh_la_LDFLAGS = -module -avoid-version
libgphoto2_ricoh_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
