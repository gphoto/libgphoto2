EXTRA_DIST = kodak.df makefile.os2

camlib_LTLIBRARIES = kodak_dc3200.la

kodak_dc3200_la_SOURCES = dc3200.c dc3200.h library.c library.h
kodak_dc3200_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
kodak_dc3200_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
