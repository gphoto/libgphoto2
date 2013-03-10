########################################################################
# Usage instructions
########################################################################
# If you only want to build a single camlib (say agfa_cl20), run
#     gmake -C camlibs agfa_cl20.la
# or
#     ( cd camlibs && make agfa_cl20.la )
#
# If you only want to install a single camlib or two, run
#     make DRIVERS="canon.la agfa_cl20.la" install
# in the camlibs/ subdir or in the libgphoto2 top-level directory.
########################################################################


########################################################################
# The only SUBDIRS here are "." (to run all-local checking the
# definition of the camlibs) and the special ones for building docs
# and stuff.
#
# Building the camlibs itself is defined in the Makefile-files
# included below.
SUBDIRS = . canon/doc konica/localization


########################################################################
# Define the common variables added to in the included Makefile-files
CLEANFILES =
EXTRA_DIST = camlib.sym
camlibdoc_DATA =
localization_DATA =
localizationdir =
noinst_DATA =
noinst_LTLIBRARIES =
EXTRA_LTLIBRARIES =


########################################################################
# All camlibs are defined as EXTRA_LTLIBRARIES. This requires that
# all camlibs have -rpath set explicitly in foobar_la_LDFLAGS.
# You're probably best off by using the camlib_* variables defined
# below.
camlib_LTLIBRARIES = $(BUILD_THESE_CAMLIBS)


########################################################################
# Define the compile/link/etc. flags common to all camlibs in one place,
# i.e. here.

camlib_dependencies = $(top_srcdir)/camlibs/camlib.sym
camlib_ldflags = -module -no-undefined -avoid-version -export-dynamic \
	-export-symbols '$(top_srcdir)/camlibs/camlib.sym' \
	-rpath '$(camlibdir)'
camlib_libadd = $(top_builddir)/libgphoto2/libgphoto2.la $(top_builddir)/libgphoto2_port/libgphoto2_port/libgphoto2_port.la 

AM_CPPFLAGS += -I$(top_srcdir) -I$(top_builddir)  -I$(top_srcdir)/libgphoto2_port -I$(top_srcdir)/libgphoto2 -I$(top_builddir)/libgphoto2

########################################################################
# You can generate this Makefile-files include list by running
# find . -name Makefile-files -exec echo include {} \; | sort | sed 's| ./| |g' 
include adc65/Makefile-files
include agfa-cl20/Makefile-files
include aox/Makefile-files
include ax203/Makefile-files
include barbie/Makefile-files
include canon/Makefile-files
include casio/Makefile-files
include clicksmart310/Makefile-files
include digigr8/Makefile-files
include digita/Makefile-files
include dimera/Makefile-files
include directory/Makefile-files
include enigma13/Makefile-files
include fuji/Makefile-files
include gsmart300/Makefile-files
include hp215/Makefile-files
include iclick/Makefile-files
include jamcam/Makefile-files
include jd11/Makefile-files
include jl2005a/Makefile-files
include jl2005c/Makefile-files
include kodak/dc120/Makefile-files
include kodak/dc210/Makefile-files
include kodak/dc240/Makefile-files
include kodak/dc3200/Makefile-files
include kodak/ez200/Makefile-files
include kodak/Makefile-files
include konica/Makefile-files
include largan/lmini/Makefile-files
include lg_gsm/Makefile-files
include mars/Makefile-files
include minolta/dimagev/Makefile-files
include minolta/Makefile-files
include mustek/Makefile-files
include panasonic/coolshot/Makefile-files
include panasonic/l859/Makefile-files
include panasonic/Makefile-files
include pccam300/Makefile-files
include pccam600/Makefile-files
include pentax/Makefile-files
include polaroid/Makefile-files
include ptp2/Makefile-files
include ricoh/Makefile-files
include samsung/Makefile-files
include sierra/Makefile-files
include sipix/Makefile-files
include smal/Makefile-files
include sonix/Makefile-files
include sonydscf1/Makefile-files
include sonydscf55/Makefile-files
include soundvision/Makefile-files
include spca50x/Makefile-files
include sq905/Makefile-files
include st2205/Makefile-files
include stv0674/Makefile-files
include stv0680/Makefile-files
include sx330z/Makefile-files
include template/Makefile-files
include topfield/Makefile-files
include toshiba/pdrm11/Makefile-files
include tp6801/Makefile-files
# End of list of Makefile-files


########################################################################
# Print list of GP_CAMLIB() definitions suitable for adding to
# configure.ac
print-camlibs: Makefile
	@for x in $(EXTRA_LTLIBRARIES); do \
		y="$$(basename "$$x" ".la")"; \
		echo "GP_CAMLIB([$$y])dnl"; \
	done


########################################################################
# Make sure that the list of camlibs defined in configure.ac using
# GP_CAMLIB(...) and the list of camlibs defined in */Makefile-files
# match.
CLEANFILES += all-src-camlibs all-def-camlibs
all-local: Makefile
	@:; \
	(for x in $(EXTRA_LTLIBRARIES) template.la; do echo "$$(basename "$$x" ".la")"; done) \
		| sort > all-src-camlibs; \
	(for x in $(ALL_DEFINED_CAMLIBS); do echo "$$x"; done) \
		| sort > all-def-camlibs; \
	if cmp all-def-camlibs all-src-camlibs; then :; else \
		diff -u all-def-camlibs all-src-camlibs; \
		echo "List of camlibs in configure.ac is out of date."; \
		echo "Have a developer update it using \"make -C camlibs print-camlibs\"."; \
		exit 1; \
	fi


########################################################################
# Shortcut for installation of a subset of camlibs
# Usage: make CAMLIBS="canon.la ptp2.la" install-camlibs

install-camlibs:
	$(MAKE) BUILD_THESE_CAMLIBS="$(CAMLIBS)" install-camlibLTLIBRARIES


########################################################################
# End of camlibs/Makefile.am
