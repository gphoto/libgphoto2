SUBDIRS = dscf55

drivers_LTLIBRARIES = libgphoto_sony_dscf1.la

CFLAGS = @CFLAGS@ @GTK_CFLAGS@ @GDK_IMLIB_CFLAGS@ 

libgphoto_sony_dscf1_la_SOURCES = tty.c command.c getuint.c \
	chotplay.c dsc_f1.c \
	tty.h command.h getuint.h chotplay.h pmp.h config.h common.h

libgphoto_sony_dscf1_la_LDFLAGS = -version-info 0:0:0

libgphoto_sony_dscf1_la_LIBADD = -lgphoto2
