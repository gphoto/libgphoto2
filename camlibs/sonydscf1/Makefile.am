camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_sony_dscf1.la

INCLUDES = -I$(top_srcdir)/include

libgphoto2_sony_dscf1_la_SOURCES = tty.c command.c getuint.c \
	chotplay.c dsc_f1.c \
	tty.h command.h getuint.h chotplay.h pmp.h config.h common.h

libgphoto2_sony_dscf1_la_LDFLAGS =\
 -avoid-version\
 -L$(shell pwd)/$(top_builddir)/libgphoto2/.libs

libgphoto2_sony_dscf1_la_LIBADD = -lgphoto2
