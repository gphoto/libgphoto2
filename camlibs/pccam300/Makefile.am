camlib_LTLIBRARIES = pccam300.la

pccam300_la_SOURCES = pccam300.c pccam300.h library.c 
pccam300_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
pccam300_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
