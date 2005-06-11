camlib_LTLIBRARIES = sx330z.la

sx330z_la_SOURCES = library.c sx330z.c sx330z.h
sx330z_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
sx330z_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
