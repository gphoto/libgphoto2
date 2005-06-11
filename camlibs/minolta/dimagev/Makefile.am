EXTRA_DIST = dimagev.df makefile.os2

camlib_LTLIBRARIES = dimagev.la

dimagev_la_SOURCES = capture.c data.c delete.c dimagev.c dimagev.h \
	download.c info.c packet.c status.c upload.c util.c
dimagev_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
dimagev_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
