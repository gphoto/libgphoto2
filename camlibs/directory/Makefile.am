EXTRA_DIST = directory.df directory.def makefile.os2 makedll.cmd

camlibdir = $(libdir)/gphoto2/$(VERSION)
camlib_LTLIBRARIES = libgphoto2_directory.la

INCLUDES =                                              \
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2

libgphoto2_directory_la_SOURCES = directory.c
libgphoto2_directory_la_LDFLAGS = -module -avoid-version
libgphoto2_directory_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
