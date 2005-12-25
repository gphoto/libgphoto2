# If you use this as template for a real camlib, make sure to
# change the "noinst_" at the beginning of the following line
# into "camlib_".
noinst_LTLIBRARIES = template.la
#camlib_LTLIBRARIES = template.la

template_la_SOURCES = template.c
template_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
template_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
