EXTRA_DIST = sonydscf1.df makefile.os2 readme todo

camlibdir = $(libdir)/gphoto2/$(VERSION)
camlib_LTLIBRARIES = libgphoto2_sony_dscf1.la

INCLUDES =                                              \
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2

libgphoto2_sony_dscf1_la_SOURCES = \
	command.c getuint.c chotplay.c sonydscf1.c sonydscf1.h \
	command.h getuint.h chotplay.h pmp.h config.h common.h

libgphoto2_sony_dscf1_la_LDFLAGS = -module -avoid-version
libgphoto2_sony_dscf1_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la 
