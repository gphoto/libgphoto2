# Process this file with automake to generate a GNU Makefile

########################################################################
# put file lists and special compile rules here

bin_SCRIPTS = gphoto2-config

EXTRA_DIST = HACKING MAINTAINERS makefile.os2 os2mak.inc TESTERS CHANGES

SUBDIRS = 			\
	libgphoto2_port 	\
	intl 			\
	libgphoto2 		\
	camlibs 		\
	tests			\
	po			\
	packaging		\
	doc

docdir = @DOC_DIR@
doc_DATA = AUTHORS COPYING README

########################################################################
# The following default options for aclocal and automake
# are required in the top level Makefile.am for the automatic 
# re-building of the build system files (only if configured with
# --enable-maintainer-mode). 
#
# It is not sufficient to have these options somewhere in autogen.sh!

ACLOCAL_AMFLAGS = -I libgphoto2_port/m4
AUTOMAKE_OPTIONS = gnu

########################################################################
# The following rules are only interesting if you want to create
# distribution packages (as source tarballs or system specific packages)

# create .tar.bz2 distribution package for convenience if the
# $(TAR) on this system supports it. else do nothing.
dist-hook:
	if [ "x${CDPATH}" != "x" ]; then \
		echo "You have set CDPATH. If make dist fails, try to unset CDPATH."; \
	fi
	-$(MAKE) dist-bzip2

dist-bzip2:
	-chmod -R a+r $(distdir)
	-tar -c --bzip2 -h -o -f $(distdir).tar.bz2 $(distdir)

if ENABLE_RPM

# create RPM packages
.PHONY: rpm
rpm: Makefile
	$(MAKE) dist
	$(MAKE) -C packaging/rpm rpm
	@echo "The RPM packages should now be found in packaging/rpm/RPMS"

endif

if MAINTAINER_MODE

# add CVS tags - used only by the project maintainer to tag the files of a
# released version
.PHONY: cvs-tags
cvs-tags: Makefile
	@echo cvs tag "@PACKAGE@-$$(echo '@VERSION@' | sed 's/\./_/g')"

endif
