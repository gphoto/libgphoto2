EXTRA_DIST = directory.df directory.def makefile.os2 makedll.cmd

camlib_LTLIBRARIES = libgphoto2_directory.la

libgphoto2_directory_la_CPPFLAGS = 			\
	$(CPPFLAGS) $(AM_CPPFLAGS)			\
	$(LIBEXIF_CFLAGS)
	
libgphoto2_directory_la_SOURCES = directory.c
libgphoto2_directory_la_LDFLAGS = -module -avoid-version
libgphoto2_directory_la_LIBADD =			\
	$(LIBEXIF_LIBS)					\
	$(top_builddir)/libgphoto2/libgphoto2.la
