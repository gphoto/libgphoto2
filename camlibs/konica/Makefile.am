SUBDIRS = localization

camlib_LTLIBRARIES = libgphoto2_konica.la

EXTRA_DIST = EXPERTS konica.df makefile.os2

INCLUDES =\
 -DLOCALIZATION=\"$(datadir)/$(PACKAGE)/konica\"	\
 -I$(top_srcdir)/intl					\
 -I$(top_srcdir)/libgphoto2				\
 -I$(top_srcdir)/libgphoto2_port/libgphoto2_port

libgphoto2_konica_la_SOURCES = \
 lowlevel.c lowlevel.h \
 konica.c konica.h \
 library.c

libgphoto2_konica_la_LDFLAGS = -module -avoid-version
libgphoto2_konica_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
