# Process this file with automake to generate a GNU Makefile

bin_SCRIPTS = gphoto2-config

EXTRA_DIST = HACKING MAINTAINERS acconfig.h autogen.sh \
	makefile.os2 os2mak.inc TESTERS

SUBDIRS = 			\
	libgphoto2_port 	\
	intl 			\
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
	if [ "x${CDPATH}" != "x" ]; then \
		echo "You have set CDPATH. If make dist fails, try to unset CDPATH."; \
	fi
	-$(MAKE) dist-bzip2

dist-bzip2:
	-chmod -R a+r $(distdir)
	-$(TAR) chojf $(distdir).tar.bz2 $(distdir)

# create RPM packages
.PHONY: rpm
rpm: Makefile
	$(MAKE) dist
	$(MAKE) -C packaging/rpm rpm

# add CVS tags - used only by the project maintainer to tag the files of a
# released version
.PHONY: cvs-tags
cvs-tags: Makefile
	@echo cvs tag "@PACKAGE@-$$(echo '@VERSION@' | sed 's/\./_/g')"

