camlib_LTLIBRARIES = lg_gsm.la

lg_gsm_la_SOURCES = library.c lg_gsm.c lg_gsm.h
lg_gsm_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
lg_gsm_la_DEPENDENCIES = $(top_srcdir)/camlibs/camlib.sym
lg_gsm_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la

camlibdoc_DATA = README.lg_gsm
EXTRA_DIST = README.lg_gsm
