########################################################################
# Usage instructions
########################################################################
# If you only want to build a single iolib (say usb.la), run
#     gmake usb.la
# or  make usb.la
#
# If you only want to install a single iolib or two, run
#     make IOLIB_LTLIST="usb.la ptpip.la" install-iolibLTLIBRARIES
########################################################################


CLEANFILES =
EXTRA_DIST =
EXTRA_LTLIBRARIES =
udevscript_PROGRAMS =


## These two definitions look obsolete:
## Specify some includes and versioning
#AM_CPPFLAGS = -I@srcdir@ $(VERSION_FLAGS)
#VERSION_FLAGS = -DLIBGPPORT_VERSION=\"@LIBGPPORT_VERSION@\" -DHAVE_TERMIOS_H

## Executable files
bin_SCRIPTS = gphoto2-port-config

# The . stands for the current dir, i.e. the iolibs to build.
# Make sure the iolibs are built before running tests on them.
SUBDIRS = po libgphoto2_port . tests doc

include gphoto-m4/Makefile-files

@GP_GETTEXT_SETUP_MK@

# Dummy target to force Automake to make the all target depend on it
all-local:
	@:

# Dummy target to force Automake to make the distclean target depend on it
distclean-local:
	@:


########################################################################
# All iolibs are defined as EXTRA_LTLIBRARIES. This requires that
# all iolibs have -rpath set explicitly in foobar_la_LDFLAGS.
# You're probably best off by using the iolib_* variables defined
# below.
iolib_LTLIBRARIES = $(IOLIB_LTLIST)


########################################################################
# Define the compile/link/etc. flags common to all iolibs in one place,
# i.e. here.
iolib_dependencies = $(top_srcdir)/iolib.sym
iolib_ldflags = -module -no-undefined -avoid-version \
	-export-dynamic \
        -export-symbols $(top_srcdir)/iolib.sym \
	-rpath '$(iolibdir)'
iolib_libadd = $(top_builddir)/libgphoto2_port/libgphoto2_port.la

AM_CPPFLAGS += -I$(top_srcdir)

include disk/Makefile-files
include ptpip/Makefile-files
include serial/Makefile-files
include usb/Makefile-files
include libusb1/Makefile-files
include usbdiskdirect/Makefile-files
include usbscsi/Makefile-files
include vusb/Makefile-files


########################################################################
# Miscellaneous stuff

EXTRA_DIST +=   		\
	AUTHORS \
	ChangeLog \
	COPYING.LIB \
	NEWS \
	README \
	iolib.sym \
	gphoto2-port-config.in

doc_DATA = AUTHORS README NEWS

ACLOCAL_AMFLAGS = -I auto-m4 -I gphoto-m4

EXTRA_DIST    += libgphoto2_port.pc.in
pkgconfig_DATA = libgphoto2_port.pc

EXTRA_DIST += libgphoto2_port-uninstalled.pc.in
noinst_DATA = libgphoto2_port-uninstalled.pc


# ship headers
nobase_include_HEADERS =	\
	gphoto2/gphoto2-port.h			\
	gphoto2/gphoto2-port-info-list.h	\
	gphoto2/gphoto2-port-log.h		\
	gphoto2/gphoto2-port-version.h		\
	gphoto2/gphoto2-port-portability.h	\
	gphoto2/gphoto2-port-result.h

EXTRA_DIST += gphoto2/gphoto2-port-library.h
EXTRA_DIST += gphoto2/gphoto2-port-locking.h
