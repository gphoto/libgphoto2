camlib_LTLIBRARIES = sonix.la

sonix_la_SOURCES = library.c sonix.c sonix.h sakar-avi-header.h
sonix_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
sonix_la_DEPENDENCIES = $(top_srcdir)/camlibs/camlib.sym
sonix_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la

camlibdoc_DATA = README.sonix
EXTRA_DIST = README.sonix
