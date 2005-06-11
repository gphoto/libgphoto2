camlib_LTLIBRARIES = mustek.la

EXTRA_DIST = AUTHOR

mustek_la_SOURCES = mdc800.c core.c image.c \
	io.c rs232.c usb.c core.h image.h \
	io.h mdc800_spec.h print.h rs232.h usb.h
mustek_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
mustek_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
