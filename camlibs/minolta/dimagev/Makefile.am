EXTRA_DIST = dimagev.df makefile.os2

camlib_LTLIBRARIES = libgphoto2_dimagev.la

INCLUDES =                                              \
        -I$(top_srcdir)/intl				\
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2

libgphoto2_dimagev_la_SOURCES = capture.c data.c delete.c dimagev.c dimagev.h \
	download.c info.c packet.c status.c upload.c util.c

libgphoto2_dimagev_la_LDFLAGS = -module -avoid-version
libgphoto2_dimagev_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
