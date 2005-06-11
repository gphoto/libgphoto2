camlib_LTLIBRARIES = aox.la

aox_la_SOURCES = library.c aox.c aox.h
aox_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
aox_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
