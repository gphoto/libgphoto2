camlib_LTLIBRARIES = fuji.la

EXTRA_DIST = PROTOCOL

fuji_la_SOURCES = library.c fuji.c fuji.h
fuji_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
fuji_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
