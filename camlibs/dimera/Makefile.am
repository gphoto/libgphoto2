EXTRA_DIST = dimera.df makefile.os2 CREDITS Protocol.txt

camlib_LTLIBRARIES = libgphoto2_dimera3500.la

INCLUDES =                                              \
        -I$(top_srcdir)/intl				\
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2			\
	-I$(top_builddir)/libgphoto2

libgphoto2_dimera3500_la_SOURCES =\
 dimera3500.c \
 mesalib.c mesalib.h\
 dimeratab.h

libgphoto2_dimera3500_la_LDFLAGS = -module -avoid-version
libgphoto2_dimera3500_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
