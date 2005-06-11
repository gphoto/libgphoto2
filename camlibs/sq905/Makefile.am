camlib_LTLIBRARIES = sq905.la

sq905_la_SOURCES = library.c sq905.c sq905.h postprocess.c 
sq905_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
sq905_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
