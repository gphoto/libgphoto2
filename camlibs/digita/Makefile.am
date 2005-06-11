EXTRA_DIST = digita.df makefile.os2

camlib_LTLIBRARIES = digita.la

digita_la_SOURCES = digita.c serial.c usb.c commands.c digita.h
digita_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
digita_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
