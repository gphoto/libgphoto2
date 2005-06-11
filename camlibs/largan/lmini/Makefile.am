camlib_LTLIBRARIES = largan.la

largan_la_SOURCES = largan.c lmini.c lmini_ccd.c lmini.h lmini_ccd.h
largan_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
largan_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
