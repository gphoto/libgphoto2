EXTRA_DIST = kodak.df makefile.os2

camlib_LTLIBRARIES = kodak_dc120.la

kodak_dc120_la_SOURCES = dc120.c dc120.h library.c library.h
kodak_dc120_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
kodak_dc120_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
