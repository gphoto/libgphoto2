EXTRA_DIST = sonydscf1.df makefile.os2 readme todo

camlib_LTLIBRARIES = sony_dscf1.la

EXTRA_sony_dscf1_la_SOURCES = dsc_f1.c
sony_dscf1_la_SOURCES = \
	command.c getuint.c chotplay.c sonydscf1.c sonydscf1.h \
	command.h getuint.h chotplay.h pmp.h common.h
sony_dscf1_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
sony_dscf1_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la 
