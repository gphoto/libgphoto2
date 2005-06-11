camlib_LTLIBRARIES = panasonic_coolshot.la

panasonic_coolshot_la_SOURCES = coolshot.c library.c library.h
panasonic_coolshot_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
panasonic_coolshot_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
