## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_srcdir)/intl \
	@GTK_CFLAGS@

CFLAGS = @CFLAGS@ `gphoto2-config --cflags` `gdk-pixbuf-config --cflags`
LDFLAGS = @LDFLAGS@ `gphoto2-config --libs` `gdk-pixbuf-config --libs`

bin_PROGRAMS = gphoto2_gtk

gphoto2_gtk_SOURCES = \
	main.c \
	support.c support.h \
	interface.c interface.h \
	callbacks.c callbacks.h \
	util.c util.h

gphoto2_gtk_LDADD = @GTK_LIBS@ $(INTLLIBS)
