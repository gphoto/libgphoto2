camlibdir = $(libdir)/gphoto2/$(VERSION)
camlib_LTLIBRARIES = libgphoto2_soundvision.la

INCLUDES =                                              \
        -I$(top_srcdir)/libgphoto2_port/libgphoto2_port \
        -I$(top_srcdir)/libgphoto2

libgphoto2_soundvision_la_SOURCES = soundvision.c agfa_cl18.c tiger_fastflicks.c commands.c 
libgphoto2_soundvision_la_LDFLAGS = -module -avoid-version
libgphoto2_soundvision_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
