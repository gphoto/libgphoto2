# Process this file with automake to generate a GNU Makefile

bin_SCRIPTS = gphoto2-config

EXTRA_DIST = @PACKAGE@.spec @PACKAGE@.spec.in \
	HACKING MAINTAINERS acconfig.h autogen.sh \
	makefile.os2 os2mak.inc

SUBDIRS = 			\
	libgphoto2_port 	\
	intl 			\
	include 		\
	libgphoto2 		\
	camlibs 		\
	frontends 		\
	tests			\
	po 			\
	doc

docdir = @DOC_DIR@
doc_DATA = AUTHORS COPYING README
