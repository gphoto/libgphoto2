# Process this file with automake to generate a GNU Makefile

########################################################################
# put file lists and special compile rules here

bin_SCRIPTS = gphoto2-config

EXTRA_DIST = MAINTAINERS TESTERS installcheck.mk
EXTRA_DIST += HACKING.md SECURITY.md

# Note: $(subdirs) lists all the directories from AC_CONFIG_SUBDIRS()
SUBDIRS = $(subdirs) libgphoto2 camlibs tests examples po packaging doc

EXTRA_DIST    += libgphoto2.pc.in
pkgconfig_DATA = libgphoto2.pc

EXTRA_DIST += libgphoto2-uninstalled.pc.in
noinst_DATA = libgphoto2-uninstalled.pc

EXTRA_DIST += ChangeLog
EXTRA_DIST += INSTALL
EXTRA_DIST += OUTDATED.txt
doc_DATA = AUTHORS COPYING NEWS
doc_DATA += ABOUT-NLS ChangeLog

EXTRA_DIST += README.md
doc_DATA   += README.md

EXTRA_DIST += RELEASE-HOWTO.md
doc_DATA   += RELEASE-HOWTO.md

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

.PHONY: source-code-check install-drivers

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
	find $(srcdir) -type f -name '*.[ch]' -exec egrep -H '(^//|[^:]//)' {} \;
	@echo "######## Finding remainders of Arnaud's comment replacement"
	find $(srcdir) -type f -name '*.[ch]' -exec egrep -H 'http:/\*' {} \;
	@echo "######## Finding explicit libintl.h references"
	find $(srcdir) -type f -name '*.[ch]' -exec egrep -H '^#.*include.*libintl\.h' {} \;


########################################################################
# Shortcut for installation of a subset of camlibs/drivers
# Usage: make CAMLIBS="canon.la ptp2.la" install-camlibs

install-camlibs:
	cd camlibs && $(MAKE) $(AM_MAKEFLAGS) CAMLIBS="$(CAMLIBS)" install-camlibs


########################################################################
# Convenience shortcuts to update the *.po translation files in both
# po/ subdirs at once and to reset the *.po translation files to "git
# HEAD" in both po/ subdirs at once.

PO_SUBDIRS =
PO_SUBDIRS += libgphoto2_port/po
PO_SUBDIRS += po

# Run "git checkout *.po" in all po/ subdirs
# which is quite useful after "make dist" to clean up "git status".
.PHONY: git-checkout-po
git-checkout-po:
	set -e; cd "$(top_srcdir)" && if test -d .git; then \
		for d in $(PO_SUBDIRS); do \
			(cd "$$d" && git checkout *.po); \
		done; \
	fi

.PHONY: pull-translations
if HAVE_RSYNC
pull-translations:
	$(RSYNC) -Lrtvz translationproject.org::tp/latest/libgphoto2/ $(top_srcdir)/po
	$(RSYNC) -Lrtvz translationproject.org::tp/latest/libgphoto2_port/ $(top_srcdir)/libgphoto2_port/po
else
pull-translations:
	echo "Error: Cannot pull translations: Could not find rsync at configure time."
	exit 1
endif

# Run "make update-po" in all po/ subdirs
.PHONY: update-po
update-po:
	@for d in $(PO_SUBDIRS); do (cd "$$d" && $(MAKE) $(AM_MAKEFLAGS) update-po)  || exit 1; done

@GP_GETTEXT_SETUP_MK@

# Dummy target to force Automake to make the all target depend on it
all-local:
	@:

# Dummy target to force Automake to make the distclean target depend on it
distclean-local:
	@:


########################################################################
# The following default options for aclocal and automake
# are required in the top level Makefile.am for the automatic
# re-building of the build system files.

ACLOCAL_AMFLAGS = -I auto-m4 -I libgphoto2_port/gphoto-m4

include libgphoto2_port/gphoto-m4/Makefile-files
