camlib_LTLIBRARIES = clicksmart310.la

clicksmart310_la_SOURCES = library.c clicksmart.c clicksmart.h spca50x-jpeg-header.h
clicksmart310_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
clicksmart310_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la

camlibdoc_DATA = README.clicksmart310
EXTRA_DIST = README.clicksmart310
