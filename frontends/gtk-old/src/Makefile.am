## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_srcdir)/intl \
	@GTK_CFLAGS@

bin_PROGRAMS = gphoto2_gtk

gphoto2_gtk_SOURCES = \
	main.c \
	support.c support.h \
	interface.c interface.h \
	callbacks.c callbacks.h \
	util.c util.h \
	gtkiconlist.c gtkiconlist.h \
	gtkitementry.c gtkitementry.h

gphoto2_gtk_LDADD = @GTK_LIBS@ $(INTLLIBS)
