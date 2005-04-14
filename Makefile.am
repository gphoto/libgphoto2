# Process this file with automake to generate a GNU Makefile

########################################################################
# put file lists and special compile rules here

bin_SCRIPTS = gphoto2-config

EXTRA_DIST = HACKING MAINTAINERS makefile.os2 os2mak.inc TESTERS CHANGES

SUBDIRS = @subdirs@ libgphoto2 camlibs tests po packaging doc

doc_DATA = AUTHORS COPYING README CHANGES NEWS

# Help for building packages against libgphoto2 without installing
# libgphoto2 first. A little hackish, but that's what you get for
# having "libgphoto2" and "libgphoto2_port" install their header
# files into $(includedir)/gphoto2.
all-local:
	rm -f gphoto2/gphoto2*.h
	mkdir -p gphoto2
	cp \
		libgphoto2/gphoto2*.h \
		libgphoto2_port/libgphoto2_port/gphoto2*.h \
		$(srcdir)/libgphoto2/gphoto2*.h \
		$(srcdir)/libgphoto2_port/libgphoto2_port/gphoto2*.h \
		gphoto2/ \
		|| true

clean-local:
	rm -f gphoto2/gphoto2*.h
	rmdir gphoto2

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
