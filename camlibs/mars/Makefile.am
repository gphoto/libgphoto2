camlib_LTLIBRARIES = mars.la

EXTRA_DIST = protocol.txt

mars_la_SOURCES = library.c mars.c mars.h
mars_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
mars_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
