gladedir = $(datadir)/gphoto/glade/

INCLUDES =\
        -I$(top_srcdir)/src \
        -I$(includedir)/src \
        $(GNOME_INCLUDEDIR) \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
        -DGPHOTO_GLADEDIR=\""$(gladedir)"\" \
        $(EXTRA_GNOME_CFLAGS)

CFLAGS = @CFLAGS@ `gphoto2-config --cflags` 
LDFLAGS = @LDFLAGS@ `gphoto2-config --libs` 

noinst_LIBRARIES = libdialogs.a

libdialogs_a_SOURCES = \
        dialog_camera_add.c \
	dialogs.h

glade_DATA = \
        camera_add.glade

EXTRA_DIST = $(glade_DATA)

