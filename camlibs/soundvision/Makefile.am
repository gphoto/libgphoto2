camlib_LTLIBRARIES = soundvision.la

soundvision_la_SOURCES = soundvision.c agfa_cl18.c \
	tiger_fastflicks.c commands.c commands.h soundvision.h
soundvision_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
soundvision_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
