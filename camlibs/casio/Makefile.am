camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_casio_qv.la

INCLUDES = -I$(top_srcdir)/include  -I$(top_srcdir)/libgphoto2

libgphoto2_casio_qv_la_SOURCES =			\
	casio-qv.c				\
	casio-qv-commands.c casio-qv-commands.h

libgphoto2_casio_qv_la_LDFLAGS =\
 -avoid-version -L$(shell pwd)/$(top_builddir)/libgphoto2/@dotlibs@

libgphoto2_casio_qv_la_LIBADD = -lgphoto2
