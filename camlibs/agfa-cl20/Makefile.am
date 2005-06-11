camlib_LTLIBRARIES = agfa_cl20.la

EXTRA_DIST = README STATUS ChangeLog protocol.txt protocol2.txt commands.txt \
	pic-header.txt RANDOM

agfa_cl20_la_SOURCES = agfa_cl20.c
agfa_cl20_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
agfa_cl20_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
