camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_fuji.la

INCLUDES = -I$(top_srcdir)/include

libgphoto2_fuji_la_SOURCES = fuji.c fuji.h fuji_exif.c exif.c

libgphoto2_fuji_la_LDFLAGS =  -avoid-version\
 -L$(shell pwd)/$(top_builddir)/libgphoto2/@dotlibs@


libgphoto2_fuji_la_LIBADD = -lgphoto2
