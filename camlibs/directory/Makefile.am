camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_directory.la

INCLUDES = -I$(top_srcdir)/include

libgphoto2_directory_la_SOURCES = directory.c directory.h

libgphoto2_directory_la_LDFLAGS = -version-info 0:0:0

libgphoto2_directory_la_LIBADD = -lgphoto2
