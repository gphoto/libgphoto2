EXTRA_DIST = Protocol Changelog

camlib_LTLIBRARIES = stv0674.la

stv0674_la_SOURCES = stv0674.c stv0674.h library.c library.h
stv0674_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
stv0674_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
