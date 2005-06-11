camlib_LTLIBRARIES = template.la

template_la_SOURCES = template.c
template_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
template_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
