EXTRA_DIST = dimera.df makefile.os2 CREDITS Protocol.txt

camlib_LTLIBRARIES = dimera3500.la

dimera3500_la_SOURCES =\
	dimera3500.c \
	mesalib.c mesalib.h\
	dimeratab.h

dimera3500_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
dimera3500_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
