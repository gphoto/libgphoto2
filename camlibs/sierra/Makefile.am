camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_sierra.la

INCLUDES = -I$(top_srcdir)/include

libgphoto2_sierra_la_SOURCES = sierra.c sierra.h library.c library.h

libgphoto2_sierra_la_LDFLAGS = -version-info 0:0:0 `gpio-config --libs`

#libgphoto2_sierra_la_LIBADD = -lgphoto2
