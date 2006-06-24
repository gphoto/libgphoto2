# Process this file with automake to generate a GNU Makefile

########################################################################
# put file lists and special compile rules here

bin_SCRIPTS = gphoto2-config

EXTRA_DIST = HACKING MAINTAINERS makefile.os2 os2mak.inc TESTERS CHANGES

# Note: @subdirs@ lists all the directories from AC_CONFIG_SUBDIRS()
SUBDIRS = @subdirs@ libgphoto2 camlibs tests po bindings packaging doc

doc_DATA = AUTHORS COPYING README CHANGES NEWS

# Create gphoto2/ directory and copy all header files into it.
#
# Reasons to do that:
#  - Both libgphoto2 and libgphoto2_port install their *.h files
#    into $(includedir)/gphoto2
#  - Example programs can use #include <gphoto2/gphoto2-foobar.h>
#    like any libgphoto2 frontend is supposed to do.
#    (Promotes clean API use)
#  - Helps build libgphoto2 frontends against libgphoto2 without
#    installing libgphoto2 first.
#
# FIXME: This breaks "make -j all". You'd have to run "make all-local"
#        manually first.
all-local:
	@echo "Creating/Updating gphoto2/ directory with gphoto2*.h files"
	@rm -f gphoto2/gphoto2*.h
	@mkdir -p gphoto2
	@cp \
		libgphoto2/gphoto2*.h \
		libgphoto2_port/libgphoto2_port/gphoto2*.h \
		$(srcdir)/libgphoto2/gphoto2*.h \
		$(srcdir)/libgphoto2_port/libgphoto2_port/gphoto2*.h \
		gphoto2/ > /dev/null 2>&1 \
		|| true

clean-local:
	rm -f gphoto2/gphoto2*.h
	rmdir gphoto2 || true

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

ACLOCAL_AMFLAGS = -I libgphoto2_port/m4


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
