camlib_LTLIBRARIES = smal.la

smal_la_SOURCES = smal.c ultrapocket.c smal.h ultrapocket.h
smal_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
smal_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
