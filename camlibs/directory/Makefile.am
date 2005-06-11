EXTRA_DIST = directory.df directory.def makefile.os2 makedll.cmd

camlib_LTLIBRARIES = directory.la

directory_la_CPPFLAGS = $(CPPFLAGS) $(AM_CPPFLAGS) $(LIBEXIF_CFLAGS)
directory_la_SOURCES = directory.c
directory_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
directory_la_LIBADD = $(LIBEXIF_LIBS) $(top_builddir)/libgphoto2/libgphoto2.la
