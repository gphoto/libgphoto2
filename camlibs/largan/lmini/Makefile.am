camlibdir = $(libdir)/gphoto2/$(VERSION)
camlib_LTLIBRARIES = libgphoto2_largan.la

INCLUDES =                                              \
        -I$(top_srcdir)/intl				\
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2

libgphoto2_largan_la_SOURCES = largan.c lmini.c lmini_ccd.c lmini.h lmini_ccd.h
libgphoto2_largan_la_LDFLAGS = -module -avoid-version
libgphoto2_largan_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
