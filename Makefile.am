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
	po			\
	packaging		\
	doc

docdir = @DOC_DIR@
doc_DATA = AUTHORS COPYING README

########################################################################
# create .tar.bz2 distribution package for convenience if the
# $(TAR) on this system supports it. else do nothing.

dist-hook:
	-$(MAKE) dist-bzip2

dist-bzip2:
	-chmod -R a+r $(distdir)
	-$(TAR) chojf $(distdir).tar.bz2 $(distdir)
