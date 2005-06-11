EXTRA_DIST = jpeg.txt jpeghead.h pdc320.html gt98x.html

camlib_LTLIBRARIES =			\
	polaroid_pdc320.la	\
	polaroid_pdc640.la	\
	polaroid_pdc700.la


polaroid_pdc320_la_SOURCES = pdc320.c pdc320.h
polaroid_pdc320_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
polaroid_pdc320_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la


polaroid_pdc640_la_SOURCES = pdc640.c jd350e.h jd350e.c \
	jd350e_red.h jd350e_blue.h dlink350f.c dlink350f.h
polaroid_pdc640_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
polaroid_pdc640_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la


polaroid_pdc700_la_SOURCES = pdc700.c
polaroid_pdc700_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
polaroid_pdc700_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
