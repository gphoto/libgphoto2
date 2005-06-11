camlib_LTLIBRARIES = adc65.la
EXTRA_DIST = README TODO Changelog

adc65_la_SOURCES = adc65.c adc65.h library.c
adc65_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
adc65_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
