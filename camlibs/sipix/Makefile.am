camlib_LTLIBRARIES = 					\
	libgphoto2_sipix_blink.la			\
	libgphoto2_sipix_blink2.la			\
	libgphoto2_sipix_web2.la

EXTRA_DIST = web2.html blink.txt

INCLUDES = \
        -I$(top_srcdir)/intl				\
	-I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2



libgphoto2_sipix_blink_la_SOURCES	= blink.c
libgphoto2_sipix_blink2_la_SOURCES	= blink2.c
libgphoto2_sipix_web2_la_SOURCES	= web2.c

libgphoto2_sipix_blink_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
libgphoto2_sipix_blink_la_LDFLAGS = -module -avoid-version
libgphoto2_sipix_blink2_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
libgphoto2_sipix_blink2_la_LDFLAGS = -module -avoid-version
libgphoto2_sipix_web2_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
libgphoto2_sipix_web2_la_LDFLAGS = -module -avoid-version
