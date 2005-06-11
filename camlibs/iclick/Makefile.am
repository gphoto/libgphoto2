camlib_LTLIBRARIES = iclick.la

iclick_la_SOURCES = library.c iclick.c iclick.h  
iclick_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
iclick_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
