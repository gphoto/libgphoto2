EXTRA_DIST = dimera.df makefile.os2 CREDITS Protocol.txt

camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_dimera3500.la

CFLAGS = @CFLAGS@ -g -Wall
LDFLAGS = @LDFLAGS@ -g
INCLUDES =                                              \
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2

libgphoto2_dimera3500_la_SOURCES =\
 dimera3500.c \
 mesalib.c mesalib.h\
 dimeratab.h

libgphoto2_dimera3500_la_LDFLAGS = -module -avoid-version
libgphoto2_dimera3500_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
