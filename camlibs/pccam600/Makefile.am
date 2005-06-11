camlib_LTLIBRARIES = pccam600.la

pccam600_la_SOURCES = pccam600.c pccam600.h library.c 
pccam600_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
pccam600_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
