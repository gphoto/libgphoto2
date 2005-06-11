EXTRA_DIST = ptp-pack.c

camlib_LTLIBRARIES = ptp2.la

ptp2_la_SOURCES = ptp.c ptp.h library.c ptp-bugs.h
ptp2_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
ptp2_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
