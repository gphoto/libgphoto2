camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_panasonic_coolshot.la

INCLUDES = -I$(top_srcdir)/include

libgphoto2_panasonic_coolshot_la_SOURCES = coolshot.c coolshot.h library.c library.h

libgphoto2_panasonic_coolshot_la_LDFLAGS =\
 -avoid-version -L$(shell pwd)/$(top_builddir)/libgphoto2/.libs

libgphoto2_panasonic_coolshot_la_LIBADD = -lgphoto2
