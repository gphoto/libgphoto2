## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_srcdir)/intl \
	@GTK_CFLAGS@

bin_PROGRAMS = project5

project5_SOURCES = \
	main.c \
	support.c support.h \
	interface.c interface.h \
	callbacks.c callbacks.h \
	tree_list_util.c tree_list_util.h

project5_LDADD = @GTK_LIBS@ $(INTLLIBS)
