## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_srcdir)/intl \
	@GTK_CFLAGS@

bin_PROGRAMS = gphoto2_gtk2

gphoto2_gtk2_SOURCES = \
	main.c \
	support.c support.h \
	interface.c interface.h \
	callbacks.c callbacks.h \
	tree_list_util.c tree_list_util.h

gphoto2_gtk2_LDADD = @GTK_LIBS@ $(INTLLIBS)
