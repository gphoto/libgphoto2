camlib_LTLIBRARIES = gsmart300.la

gsmart300_la_SOURCES = gsmart300.c gsmart300.h library.c \
                               gsmart300-jpeg-header.h 
gsmart300_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
gsmart300_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
