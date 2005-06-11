camlib_LTLIBRARIES = kodak_ez200.la

EXTRA_DIST = Protocol.txt

kodak_ez200_la_SOURCES = library.c ez200.c ez200.h
kodak_ez200_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
kodak_ez200_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
