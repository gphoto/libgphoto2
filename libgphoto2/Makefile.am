CLEANFILES =
EXTRA_DIST =
noinst_DATA =


# included by all *.c files containing translated string literals as
# libgphoto2/i18n.h
EXTRA_DIST += i18n.h


# dummy file needed by gtk-doc
EXTRA_DIST += gphoto2-library.c


lib_LTLIBRARIES             = libgphoto2.la
libgphoto2_la_CFLAGS        =
libgphoto2_la_CPPFLAGS      =
libgphoto2_la_DEPENDENCIES  =
libgphoto2_la_LDFLAGS       =
libgphoto2_la_LIBADD        =
libgphoto2_la_SOURCES       =

libgphoto2_la_CPPFLAGS     += -I$(top_srcdir)
libgphoto2_la_CPPFLAGS     += -I$(top_builddir)

libgphoto2_la_CPPFLAGS     += -I$(top_srcdir)/libgphoto2_port
libgphoto2_la_CPPFLAGS     += -I$(top_builddir)/libgphoto2_port

libgphoto2_la_CPPFLAGS     += $(AM_CPPFLAGS)

libgphoto2_la_LDFLAGS      += -no-undefined

# Notes about the list of exported symbols:
#    gp_* also contains internal symbols which are intended for
#         INTERNAL USE ONLY and should be converted into gpi_* symbols
#    gpi_* are for libgphoto2/camlib internal use only
#          NEVER USE gpi_* in frontends!
#    gp_* gphoto2 API, used in
#         - frontends
#           BE AWARE that NOT ALL gp_* FUNCTIONS ARE CONSIDERED PUBLIC.
#
# Yes, this is messy, but we are going to clean this up.

# Keep on using libgphoto2.sym until we actually have a libgphoto2.ver
# version script.
# EXTRA_DIST                 += libgphoto2.ver
EXTRA_DIST                 += libgphoto2.sym
# if HAVE_ASM_SYMVER
# libgphoto2_la_DEPENDENCIES += libgphoto2.ver
# libgphoto2_la_LDFLAGS      += -Wl,--version-script=$(srcdir)/libgphoto2.ver
# else
libgphoto2_la_DEPENDENCIES += libgphoto2.sym
libgphoto2_la_LDFLAGS      += -export-symbols $(srcdir)/libgphoto2.sym
# endif

libgphoto2_la_LDFLAGS      += -version-info @LIBGPHOTO2_VERSION_INFO@

libgphoto2_la_LIBADD       += $(libgphoto2_port_la)

# The libtool docs describe these params, but they don't build.
#	"-dlopen" self \
#	"-dlopen" $(top_builddir)/camlibs/canon/....la \
#	"-dlopen" $(top_builddir)/camlibs/ptp2/....la

libgphoto2_la_CPPFLAGS     += $(LTDLINCL)
libgphoto2_la_LIBADD       += $(LIBLTDL)

libgphoto2_la_CPPFLAGS     += $(LIBEXIF_CFLAGS)
libgphoto2_la_LIBADD       += $(LIBEXIF_LIBS)

libgphoto2_la_LIBADD       += -lm
libgphoto2_la_LIBADD       += $(INTLLIBS)

libgphoto2_la_SOURCES      += gphoto2-abilities-list.c
libgphoto2_la_SOURCES      += ahd_bayer.c
libgphoto2_la_SOURCES      += bayer.c
libgphoto2_la_SOURCES      += bayer.h
libgphoto2_la_SOURCES      += bayer-types.h
libgphoto2_la_SOURCES      += gphoto2-camera.c
libgphoto2_la_SOURCES      += gphoto2-context.c
libgphoto2_la_SOURCES      += exif.c
libgphoto2_la_SOURCES      += exif.h
libgphoto2_la_SOURCES      += gphoto2-file.c
libgphoto2_la_SOURCES      += gphoto2-filesys.c
libgphoto2_la_SOURCES      += gamma.c
libgphoto2_la_SOURCES      += gamma.h
libgphoto2_la_SOURCES      += jpeg.c
libgphoto2_la_SOURCES      += jpeg.h
libgphoto2_la_SOURCES      += gphoto2-list.c
libgphoto2_la_SOURCES      += gphoto2-result.c
libgphoto2_la_SOURCES      += gphoto2-version.c
libgphoto2_la_SOURCES      += gphoto2-setting.c
libgphoto2_la_SOURCES      += gphoto2-widget.c

libgphoto2_la_DEPENDENCIES += $(top_srcdir)/gphoto2/gphoto2-version.h


CLEANFILES  += libgphoto2-buildflags.stamp
noinst_DATA += libgphoto2-buildflags.stamp
libgphoto2-buildflags.stamp: libgphoto2.la
	@test -z "$(libgphoto2_la_CFLAGS)"   || printf "  %s %-8s %s\n" lg2 CFLAGS   "$(libgphoto2_la_CFLAGS)"
	@test -z "$(libgphoto2_la_CPPFLAGS)" || printf "  %s %-8s %s\n" lg2 CPPFLAGS "$(libgphoto2_la_CPPFLAGS)"
	@test -z "$(libgphoto2_la_LIBADD)"   || printf "  %s %-8s %s\n" lg2 LIBADD   "$(libgphoto2_la_LIBADD)"
	@test -z "$(libgphoto2_la_LDFLAGS)"  || printf "  %s %-8s %s\n" lg2 LDFLAGS  "$(libgphoto2_la_LDFLAGS)"
	@date > $@
