camlibdir = $(prefix)/lib/gphoto2
camlib_LTLIBRARIES = libgphoto2_panasonic_dc1000.la libgphoto2_panasonic_dc1580.la

INCLUDES = -I$(top_srcdir)/include

libgphoto2_panasonic_dc1000_la_SOURCES = dc1000.c dc1000.h dc.c dc.h

libgphoto2_panasonic_dc1000_la_LDFLAGS = -version-info 0:0:0 `gpio-config --libs`

libgphoto2_panasonic_dc1580_la_SOURCES = dc1580.c dc1580.h dc.c dc.h dc1000.h

libgphoto2_panasonic_dc1580_la_LDFLAGS = -version-info 0:0:0 `gpio-config --libs`
