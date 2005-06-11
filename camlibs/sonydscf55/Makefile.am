camlib_LTLIBRARIES = sony_dscf55.la

sony_dscf55_la_SOURCES = sony.c sony.h camera.c nls.h
sony_dscf55_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
sony_dscf55_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
