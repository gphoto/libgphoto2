EXTRA_DIST = dimera.df makefile.os2 CREDITS Protocol.txt

camlib_LTLIBRARIES = libgphoto2_dimera3500.la

libgphoto2_dimera3500_la_SOURCES =\
 dimera3500.c \
 mesalib.c mesalib.h\
 dimeratab.h

libgphoto2_dimera3500_la_LDFLAGS = -module -avoid-version
libgphoto2_dimera3500_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
