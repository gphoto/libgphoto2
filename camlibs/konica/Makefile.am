camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_konica.la

CFLAGS = @CFLAGS@ `glib-config --cflags`
LDFLAGS = @LDFLAGS@ `glib-config --libs`
EXTRA_DIST = ChangeLog EXPERTS
INCLUDES = -I$(top_srcdir)/include

libgphoto2_konica_la_SOURCES = \
	lowlevel.c lowlevel.h \
	konica.c konica.h \
	library.c library.h

libgphoto2_konica_la_LDFLAGS = -version-info 0:0:0
