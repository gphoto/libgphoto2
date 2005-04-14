EXTRA_DIST = kodak.df makefile.os2

camlib_LTLIBRARIES = libgphoto2_kodak_dc120.la

libgphoto2_kodak_dc120_la_SOURCES = dc120.c dc120.h library.c library.h
libgphoto2_kodak_dc120_la_LDFLAGS = -module -avoid-version
libgphoto2_kodak_dc120_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
