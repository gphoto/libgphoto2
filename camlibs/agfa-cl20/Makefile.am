camlib_LTLIBRARIES = libgphoto2_agfa_cl20.la

INCLUDES =                                              \
	-I$(top_srcdir)/intl				\
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port	\
        -I$(top_srcdir)/libgphoto2

EXTRA_DIST = README STATUS ChangeLog protocol.txt protocol2.txt commands.txt \
	pic-header.txt RANDOM

libgphoto2_agfa_cl20_la_SOURCES = agfa_cl20.c
libgphoto2_agfa_cl20_la_LDFLAGS = -module -avoid-version
libgphoto2_agfa_cl20_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
