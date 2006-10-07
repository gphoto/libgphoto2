EXTRA_DIST = main.c compile README.barbie

camlib_LTLIBRARIES = barbie.la

barbie_la_SOURCES = barbie.c barbie.h library.c library.h
barbie_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
barbie_la_DEPENDENCIES = $(top_srcdir)/camlibs/camlib.sym
barbie_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la

camlibdoc_DATA = README.barbie
