## Process this file with automake to produce Makefile.in

## Specify some includes and versioning
INCLUDES = -I@srcdir@ $(VERSION_FLAGS)
VERSION_FLAGS = -DLIBGPPORT_VERSION=\"@LIBGPPORT_VERSION@\" -DHAVE_TERMIOS_H

## Executable files
bin_SCRIPTS = gphoto2-port-config

SUBDIRS = po libgphoto2_port test @IOLIB_SUBDIRS@ doc m4

EXTRA_DIST =   		\
	AUTHORS \
	ChangeLog \
	COPYING.LIB \
	NEWS \
	README \
	gphoto2-port-config.in \
	makefile.os2 lgp2port.df

doc_DATA = AUTHORS README NEWS

ACLOCAL_AMFLAGS = -I m4
