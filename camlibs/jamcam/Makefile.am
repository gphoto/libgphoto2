camlib_LTLIBRARIES = jamcam.la

jamcam_la_SOURCES = jamcam.c library.c library.h
jamcam_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
jamcam_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
