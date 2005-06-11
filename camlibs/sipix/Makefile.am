camlib_LTLIBRARIES = 			\
	sipix_blink.la			\
	sipix_blink2.la			\
	sipix_web2.la

EXTRA_DIST = web2.html blink.txt xjpeg.c blink2.txt


sipix_blink_la_SOURCES	= blink.c
sipix_blink_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
sipix_blink_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la


sipix_blink2_la_SOURCES	= blink2.c
sipix_blink2_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
sipix_blink2_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la @LIBJPEG@


sipix_web2_la_SOURCES	= web2.c
sipix_web2_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
sipix_web2_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
