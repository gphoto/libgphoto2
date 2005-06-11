EXTRA_DIST = stv0680.df makefile.os2 CREDITS LICENCE README.pdf 680_commands_and_communication_sdk.pdf.gz

camlib_LTLIBRARIES = stv0680.la

stv0680_la_SOURCES = stv0680.c stv0680.h library.c library.h \
	bayer.c bayer.h sharpen.c sharpen.h \
	saturate.c saturate.h \
	demosaic_sharpen.h demosaic_sharpen.c

stv0680_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
stv0680_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
