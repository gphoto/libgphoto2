camlib_LTLIBRARIES = libgphoto2_casio_qv.la
EXTRA_DIST = PROTOCOL.txt

INCLUDES =                                              \
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2			\
	-I$(top_builddir)/libgphoto2

libgphoto2_casio_qv_la_SOURCES =		\
	casio-qv.c				\
	casio-qv-commands.c casio-qv-commands.h \
	camtojpeg.c camtojpeg.h			\
	cam2jpgtab.h jpegtab_f.h		\
	ycctoppm.c ycctoppm.h

libgphoto2_casio_qv_la_LDFLAGS = -module -avoid-version
libgphoto2_casio_qv_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
