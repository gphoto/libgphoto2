EXTRA_DIST = agfa.df

camlibdir = $(prefix)/lib/gphoto2/$(VERSION)
camlib_LTLIBRARIES = libgphoto2_agfa.la

INCLUDES =						\
	-I$(top_srcdir)/libgphoto2_port/libgphoto2_port	\
	-I$(top_srcdir)/libgphoto2

libgphoto2_agfa_la_SOURCES = agfa.c commands.c agfa.h
libgphoto2_agfa_la_LDFLAGS = -module -avoid-version
libgphoto2_agfa_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
