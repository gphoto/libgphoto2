camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_template.la

INCLUDES = -I$(top_srcdir)/include

libgphoto2_template_la_SOURCES = template.c template.h

libgphoto2_template_la_LDFLAGS =\
 -avoid-version\
 -L$(shell pwd)/$(top_builddir)/libgphoto2/.libs

libgphoto2_template_la_LIBADD = -lgphoto2
