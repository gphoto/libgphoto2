camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_dimera3500.la

CFLAGS = @CFLAGS@ -g -Wall
LDFLAGS = @LDFLAGS@ -g
INCLUDES = -I$(top_srcdir)/include

libgphoto2_dimera3500_la_SOURCES =\
 dimera3500.c dimera3500.h\
 mesalib.c mesalib.h\
 dimeratab.h

libgphoto2_dimera3500_la_LDFLAGS =\
 -avoid-version -g\
 -L$(shell pwd)/$(top_builddir)/libgphoto2/.libs

libgphoto2_dimera3500_la_LIBADD = -lgphoto2
