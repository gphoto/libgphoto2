EXTRA_DIST = kodak.df makefile.os2

camlib_LTLIBRARIES = kodak_dc210.la

kodak_dc210_la_SOURCES = dc210.c dc210.h library.c library.h
kodak_dc210_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
kodak_dc210_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
