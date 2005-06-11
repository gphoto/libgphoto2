EXTRA_DIST = panasonic.df makefile.os2 dc1000.df dc1580.df

SUBDIRS = coolshot l859

camlib_LTLIBRARIES =\
	panasonic_dc1000.la\
	panasonic_dc1580.la

panasonic_dc1000_la_SOURCES = dc1000.c dc1000.h dc.c dc.h
panasonic_dc1000_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
panasonic_dc1000_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la

panasonic_dc1580_la_SOURCES = dc1580.c dc1580.h dc.c dc.h dc1000.h
panasonic_dc1580_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
panasonic_dc1580_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
