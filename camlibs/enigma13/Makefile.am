camlib_LTLIBRARIES = 					\
	libgphoto2_enigma13.la

EXTRA_DIST = STATUS protocol.txt

INCLUDES = \
        -I$(top_srcdir)/intl				\
	-I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2			\
	-I$(top_builddir)/libgphoto2

libgphoto2_enigma13_la_SOURCES	= enigma13.c
libgphoto2_enigma13_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
libgphoto2_enigma13_la_LDFLAGS = -module -avoid-version
