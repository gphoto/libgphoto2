camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_directory.la

INCLUDES = -I$(top_srcdir)/include

libgphoto2_directory_la_SOURCES = directory.c

libgphoto2_directory_la_LDFLAGS =\
 -avoid-version\
 -L$(shell pwd)/$(top_builddir)/libgphoto2/@dotlibs@

libgphoto2_directory_la_LIBADD = -lgphoto2
