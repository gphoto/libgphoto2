# Process this file with automake to generate a GNU Makefile

########################################################################
# put file lists and special compile rules here

bin_SCRIPTS = gphoto2-config

EXTRA_DIST = HACKING MAINTAINERS TESTERS installcheck.mk

# Note: @subdirs@ lists all the directories from AC_CONFIG_SUBDIRS()
SUBDIRS = @subdirs@ libgphoto2 camlibs tests examples po packaging doc

EXTRA_DIST    += libgphoto2.pc.in
pkgconfig_DATA = libgphoto2.pc

EXTRA_DIST += libgphoto2-uninstalled.pc.in
noinst_DATA = libgphoto2-uninstalled.pc

EXTRA_DIST += ChangeLog
EXTRA_DIST += INSTALL
doc_DATA = AUTHORS COPYING NEWS
doc_DATA += ABOUT-NLS ChangeLog

EXTRA_DIST += README.in README
doc_DATA   += README

EXTRA_DIST += README.packaging
doc_DATA   += README.packaging

nobase_include_HEADERS =	\
	gphoto2/gphoto2.h		\
	gphoto2/gphoto2-abilities-list.h\
	gphoto2/gphoto2-camera.h	\
	gphoto2/gphoto2-context.h	\
	gphoto2/gphoto2-file.h		\
	gphoto2/gphoto2-filesys.h	\
	gphoto2/gphoto2-library.h	\
	gphoto2/gphoto2-list.h		\
	gphoto2/gphoto2-result.h	\
	gphoto2/gphoto2-setting.h	\
	gphoto2/gphoto2-version.h	\
	gphoto2/gphoto2-widget.h

DISTCLEANFILES = libgphoto2/gphoto2-endian.h

# we use just ONE .PHONY target in order not to confuse automake

.PHONY: rpm cvs-tags source-code-check install-drivers

########################################################################
# source code check maintenance task
#
#   This finds code patterns we don't want to have.
#
#   Use it to get rid of existing patterns and make sure they aren't
#   reintroduced later.
#
#   The output is meant to be read and interpreted by human programmers.

source-code-check:
	@echo "######## Finding // comments"
	find . -type f -name '*.[ch]' -exec egrep -H '(^//|[^:]//)' {} \;
	@echo "######## Finding remainders of Arnaud's comment replacement"
	find . -type f -name '*.[ch]' -exec egrep -H 'http:/\*' {} \;
	@echo "######## Finding explicit libintl.h references"
	find . -type f -name '*.[ch]' -exec egrep -H '^#.*include.*libintl\.h' {} \;


########################################################################
# Shortcut for installation of a subset of camlibs/drivers
# Usage: make CAMLIBS="canon.la ptp2.la" install-camlibs

install-camlibs:
	cd camlibs && $(MAKE) CAMLIBS="$(CAMLIBS)" install-camlibs


########################################################################
# The following default options for aclocal and automake
# are required in the top level Makefile.am for the automatic 
# re-building of the build system files.

ACLOCAL_AMFLAGS = -I auto-m4 -I m4m


########################################################################
# The following rules are only interesting if you want to create
# distribution packages (as source tarballs or system specific packages)

if ENABLE_RPM

# create RPM packages
rpm: Makefile
	$(MAKE) dist
	$(MAKE) -C packaging/rpm rpm
	@echo "The RPM packages should now be found in packaging/rpm/RPMS"

else
rpm:
	@echo "Dummy $@ target"
endif

# add CVS tags - used only by the project maintainer to tag the files of a
# released version
cvs-tags: Makefile
	@echo cvs tag "@PACKAGE@-$$(echo '@VERSION@' | sed -e 's/\./_/g' -e 's/rc/-rc/g')"
