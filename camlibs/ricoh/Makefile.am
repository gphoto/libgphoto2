camlib_LTLIBRARIES = ricoh.la ricoh_g3.la

EXTRA_DIST = g3.txt

ricoh_la_SOURCES = library.c ricoh.c ricoh.h crctab.h
ricoh_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
ricoh_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la

ricoh_g3_la_SOURCES = g3.c
ricoh_g3_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
ricoh_g3_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
