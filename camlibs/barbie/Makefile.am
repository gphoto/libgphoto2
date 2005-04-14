EXTRA_DIST = barbie.df makefile.os2 main.c compile

camlib_LTLIBRARIES = libgphoto2_barbie.la

libgphoto2_barbie_la_SOURCES = barbie.c barbie.h library.c library.h
libgphoto2_barbie_la_LDFLAGS = -module -avoid-version
libgphoto2_barbie_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
