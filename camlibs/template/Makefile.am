camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_template.la

INCLUDES = -I$(top_srcdir)/include  -I$(top_srcdir)/libgphoto2

libgphoto2_template_la_SOURCES = template.c

libgphoto2_template_la_LDFLAGS =\
 -module -avoid-version\
 -L$(shell pwd)/$(top_builddir)/libgphoto2/@dotlibs@

libgphoto2_template_la_LIBADD = -lgphoto2
