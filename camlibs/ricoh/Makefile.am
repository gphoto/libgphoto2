camlib_LTLIBRARIES = libgphoto2_ricoh.la libgphoto2_ricoh_g3.la

EXTRA_DIST = g3.txt

INCLUDES =                                              \
        -I$(top_srcdir)/intl				\
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2			\
	-I$(top_builddir)/libgphoto2

libgphoto2_ricoh_la_SOURCES = library.c ricoh.c ricoh.h crctab.h
libgphoto2_ricoh_la_LDFLAGS = -module -avoid-version
libgphoto2_ricoh_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la

libgphoto2_ricoh_g3_la_SOURCES = g3.c
libgphoto2_ricoh_g3_la_LDFLAGS = -module -avoid-version
libgphoto2_ricoh_g3_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
