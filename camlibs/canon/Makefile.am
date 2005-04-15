SUBDIRS = doc

EXTRA_DIST = .indent.pro canon.df makefile.os2 ChangeLog urb-interpret

# link different versions as a demo
camlib_LTLIBRARIES = canon.la libcanon.la libgphoto2_canon.la


# "classic" version:
# - exports ALL symbols. BAD. (we only need 3!)
# - doesn't define -no-undefine (no Win32)
# - no nice short name (makes renaming the exported symbols to
#   canon_LTX_<symbol> slightly more difficult without ## macros)
libgphoto2_canon_la_SOURCES =	\
	canon.c canon.h		\
	library.c library.h	\
	serial.c serial.h	\
	usb.c usb.h		\
	crc.c crc.h		\
	util.c util.h
libgphoto2_canon_la_SOURCES =\
canon.c canon.h\
library.c library.h\
serial.c serial.h \
usb.c usb.h \
crc.c crc.h \
util.c util.h
libgphoto2_canon_la_LDFLAGS = -module -avoid-version
libgphoto2_canon_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la


# "common" version:
# + only export the 3 symbols
# + -no-undefined, (i.e. Win32 support)
# + doesn't need lengthy definition in every camlib's Makefile.am
# - no nice short name (makes renaming the exported symbols to
#   libcanon_LTX_<symbol> slightly more complicated without ## macros)
#   - makes renaming the exported symbols to canon_LTX_<symbol>
#     tedious without ## macros
#   - makes it impossible to have the same name the module filename and
#     the camlib name returned by camera_id()
libcanon_la_SOURCES =	\
	canon.c canon.h		\
	library.c library.h	\
	serial.c serial.h	\
	usb.c usb.h		\
	crc.c crc.h		\
	util.c util.h
libcanon_la_LDFLAGS = $(CAMLIB_LDFLAGS)
libcanon_la_LIBADD  = $(CAMLIB_LIBS)


# "ideal" version:
# + only export the 3 symbols
# + -no-undefined, (i.e. Win32 support)
# - needs lengthy definition in every camlib's Makefile.am
# + nice short name
#   + makes renaming the exported symbols to canon_LTX_<symbol> realistic
#     without ## macros
#   + makes it possible to have the same name the module filename and
#     the camlib name returned by camera_id()
canon_la_SOURCES =	\
	canon.c canon.h		\
	library.c library.h	\
	serial.c serial.h	\
	usb.c usb.h		\
	crc.c crc.h		\
	util.c util.h
canon_la_LDFLAGS = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols $(top_srcdir)/camlibs/camlib.sym
canon_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la


