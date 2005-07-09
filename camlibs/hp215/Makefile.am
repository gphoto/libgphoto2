camlib_LTLIBRARIES = hp215.la

hp215_la_SOURCES = hp215.c hp215.h crctab.h
hp215_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
hp215_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
