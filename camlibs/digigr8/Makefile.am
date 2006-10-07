camlib_LTLIBRARIES = digigr8.la

digigr8_la_SOURCES = library.c digigr8.c digigr8.h digi_postprocess.c 
digigr8_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
digigr8_la_DEPENDENCIES = $(top_srcdir)/camlibs/camlib.sym
digigr8_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la

camlibdoc_DATA = README.digigr8
EXTRA_DIST = README.digigr8
