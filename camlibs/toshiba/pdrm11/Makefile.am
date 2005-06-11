camlib_LTLIBRARIES = toshiba_pdrm11.la

toshiba_pdrm11_la_SOURCES = library.c pdrm11.c pdrm11.h
toshiba_pdrm11_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
toshiba_pdrm11_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
