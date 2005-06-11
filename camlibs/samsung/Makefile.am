camlib_LTLIBRARIES = samsung.la

samsung_la_SOURCES = samsung.c
samsung_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
samsung_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
