## Process this file with automake to produce Makefile.in

## Specify some includes and versioning
INCLUDES = -I@srcdir@ $(VERSION_FLAGS)
VERSION_FLAGS = -DLIBGPPORT_VERSION=\"@LIBGPPORT_VERSION@\" -DHAVE_TERMIOS_H

## Executable files
bin_SCRIPTS = gphoto2-port-config

SUBDIRS = po intl libgphoto2_port test @IOLIB_SUBDIRS@ doc m4

EXTRA_DIST = 		\
	AUTHORS \
	ChangeLog \
	COPYING.LIB \
	NEWS \
	README \
	libgphoto2_port.spec \
	gphoto2-port-config.in \
	acconfig.h autogen.sh \
	makefile.os2 lgp2port.df

docdir = @DOC_DIR@
doc_DATA = AUTHORS README NEWS
