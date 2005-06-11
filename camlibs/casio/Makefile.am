camlib_LTLIBRARIES = casio_qv.la
EXTRA_DIST = PROTOCOL.txt

casio_qv_la_SOURCES =		\
	casio-qv.c				\
	casio-qv-commands.c casio-qv-commands.h \
	camtojpeg.c camtojpeg.h			\
	cam2jpgtab.h jpegtab_f.h		\
	ycctoppm.c ycctoppm.h

casio_qv_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
casio_qv_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
