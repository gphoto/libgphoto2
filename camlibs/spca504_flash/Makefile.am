camlib_LTLIBRARIES = libgphoto2_spca504_flash.la

INCLUDES =                                              \
        -I$(top_srcdir)/intl				\
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2			\
	-I$(top_builddir)/libgphoto2

libgphoto2_spca504_flash_la_SOURCES = spca504_flash.c \
				      spca504_flash.h library.c
libgphoto2_spca504_flash_la_LDFLAGS = -module -avoid-version
libgphoto2_spca504_flash_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
