if ENABLED_GP2DDB
flexbison_PROGRAMS_ = test-ddb
flexbison_check_SCRIPTS_ = check-ddb.sh
flexbison_BUILT_SOURCES_ = ddb-txt.tab.c ddb-txt.tab.h ddb-txt.yy.c
flexbison_CLEANFILES_ = ddb-txt.output
endif

check_PROGRAMS = $(flexbison_PROGRAMS_)

check_SCRIPTS = \
	$(flexbison_check_SCRIPTS_)

EXTRA_DIST = \
	README.ddb \
	check-ddb.sh.in \
	ddb-txt.l \
	ddb-txt.y

INSTALL_TESTS = $(check_SCRIPTS)

# TESTS = $(check_SCRIPTS)

BUILT_SOURCES = $(flexbison_BUILT_SOURCES_)
CLEANFILES = $(check_SCRIPTS) $(BUILT_SOURCES) $(flexbison_CLEANFILES_) gp2ddb.txt

test_ddb_SOURCES = $(flexbison_BUILT_SOURCES_) test-ddb.c ddb-common.c ddb-common.h
test_ddb_LDADD = \
	-lfl \
	$(top_builddir)/libgphoto2/libgphoto2.la \
	$(top_builddir)/libgphoto2_port/libgphoto2_port/libgphoto2_port.la \
	$(LIBLTDL) \
	$(LIBEXIF_LIBS) \
	$(INTLLIBS)

ddb-txt.yy.c: ddb-txt.l ddb-txt.tab.h
	$(FLEX) -o$@ $<

ddb-txt.tab.h: ddb-txt.tab.c
ddb-txt.tab.c ddb-txt.tab.h: ddb-txt.y
	$(BISON) -v $<

check-ddb.sh: check-ddb.sh.in $(top_builddir)/packaging/generic/print-camera-list test-ddb

clean-local:
	rm -f *.s *.i

$(top_builddir)/packaging/generic/print-camera-list:
	cd $(top_builddir)/packaging/generic && $(MAKE) print-camera-list

include $(top_srcdir)/installcheck.mk

