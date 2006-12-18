# Process this file with automake to generate a GNU Makefile

########################################################################
# put file lists and special compile rules here

bin_SCRIPTS = gphoto2-config

EXTRA_DIST = HACKING MAINTAINERS TESTERS installcheck.mk

# Note: @subdirs@ lists all the directories from AC_CONFIG_SUBDIRS()
SUBDIRS = @subdirs@ libgphoto2 camlibs tests po packaging doc

doc_DATA = AUTHORS COPYING README NEWS

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

.PHONY: rpm cvs-tags source-code-check

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
# The following default options for aclocal and automake
# are required in the top level Makefile.am for the automatic 
# re-building of the build system files.

ACLOCAL_AMFLAGS = -I m4m


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
