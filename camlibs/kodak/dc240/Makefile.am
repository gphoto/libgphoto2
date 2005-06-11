EXTRA_DIST = kodak.df makefile.os2

camlib_LTLIBRARIES = kodak_dc240.la

kodak_dc240_la_SOURCES = dc240.c dc240.h library.c library.h
kodak_dc240_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
kodak_dc240_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
