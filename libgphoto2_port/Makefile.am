## Process this file with automake to produce Makefile.in

## Specify some includes and versioning

AM_CPPFLAGS = -I@srcdir@ $(VERSION_FLAGS)

VERSION_FLAGS = -DLIBGPPORT_VERSION=\"@LIBGPPORT_VERSION@\" -DHAVE_TERMIOS_H

## Executable files
bin_SCRIPTS = gphoto2-port-config

# Note: @subdirs@ lists all the directories from AC_CONFIG_SUBDIRS()
SUBDIRS = po @subdirs@ libgphoto2_port test @IOLIB_SUBDIRS@ doc m4

EXTRA_DIST =   		\
	AUTHORS \
	ChangeLog \
	COPYING.LIB \
	NEWS \
	README \
	iolib.sym \
	gphoto2-port-config.in

doc_DATA = AUTHORS README NEWS

ACLOCAL_AMFLAGS = -I m4

# ship headers
nobase_include_HEADERS =	\
	gphoto2/gphoto2-port.h			\
	gphoto2/gphoto2-port-info-list.h	\
	gphoto2/gphoto2-port-log.h		\
	gphoto2/gphoto2-port-version.h		\
	gphoto2/gphoto2-port-portability.h	\
	gphoto2/gphoto2-port-result.h

EXTRA_DIST += gphoto2/gphoto2-port-library.h


# Compatibility for header inclusions.
#
# - Old code has -I/usr/include/gphoto2
#   #include <gphoto2-camera.h>
#
# - New code has -I/usr/include
#   #include <gphoto2/gphoto2-camera.h>

install-data-local:
	rm -f $(DESTDIR)$(includedir)/gphoto2/gphoto2
	$(INSTALL) -m 0755 -d $(DESTDIR)$(includedir)/gphoto2
	$(LN_S) . $(DESTDIR)$(includedir)/gphoto2/gphoto2

uninstall-local:
	rm -f $(DESTDIR)$(includedir)/gphoto2/gphoto2
