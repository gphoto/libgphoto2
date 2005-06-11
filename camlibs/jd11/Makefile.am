EXTRA_DIST = jd11.html

camlib_LTLIBRARIES = jd11.la

jd11_la_SOURCES = jd11.c serial.h serial.c decomp.h decomp.c
jd11_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
jd11_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
