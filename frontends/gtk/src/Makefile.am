## Process this file with automake to produce Makefile.in
SUBDIRS = \
	dialogs

INCLUDES = \
	-I$(top_srcdir)/intl \
	$(GNOME_INCLUDEDIR) \
	$(EXTRA_GNOME_CFLAGS) \
	@GTK_CFLAGS@

CFLAGS = @CFLAGS@ `gphoto2-config --cflags` `gpio-config --cflags` `gdk-pixbuf-config --cflags`
LDFLAGS = @LDFLAGS@ `gphoto2-config --libs` `gpio-config --libs` `gdk-pixbuf-config --libs` 
bin_PROGRAMS = gphoto2_gtk2

gphoto2_gtk2_SOURCES = \
	main.c \
	globals.h \
	support.c support.h \
	interface.c interface.h \
	callbacks.c callbacks.h \
	tree_list_util.c tree_list_util.h \
	camera_util.c camera_util.h \
	gp_util.c gp_util.h

gphoto2_gtk2_LDADD = \
	@GTK_LIBS@ \
	$(INTLLIBS) \
	$(GNOMEUI_LIBS) \
	$(EXTRA_GNOME_LIBS) \
