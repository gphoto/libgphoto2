if HAVE_FLEX_BISON
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
	check-ddb.in \
	ddb-txt.l \
	ddb-txt.y

# FIXME: Only run this if not cross-compiling
installcheck-local: $(check_SCRIPTS)
	@total=0; failed=0; success=0; \
	echo "Running \"installcheck\" test cases..."; \
	for script in $(check_SCRIPTS); do \
		echo "Test case: $$script"; \
		total="$$(expr $$total + 1)"; \
		if "./$$script"; then \
			echo "SUCCEEDED: $$script"; \
			success="$$(expr $$success + 1)"; \
		else \
			s="$$?"; \
			echo "FAILED (return code $$s): $$script"; \
			failed="$$(expr $$failed + 1)"; \
		fi; \
	done; \
	echo "Test summary: $$success succeeded, $$failed failed, $$total in total."; \
	if test "$$failed" -gt 0; then \
		echo "Error: One or more \"installcheck\" testcases have failed."; \
		exit 1; \
	fi

# TESTS = $(check_SCRIPTS)

BUILT_SOURCES = $(flexbison_BUILT_SOURCES_)
CLEANFILES = $(check_SCRIPTS) $(BUILT_SOURCES) $(flexbison_CLEANFILES_) gp2ddb.txt

#if HAVE_FLEX_BISON
test_ddb_SOURCES = $(flexbison_BUILT_SOURCES_) ddb-common.c ddb-common.h
test_ddb_LDADD = \
	-lfl \
	$(top_builddir)/libgphoto2/libgphoto2.la \
	$(top_builddir)/libgphoto2_port/libgphoto2_port/libgphoto2_port.la \
	$(LIBEXIF_LIBS) \
	$(INTLLIBS)

%.yy.c: %.l %.tab.h
	$(FLEX) -o$@ $<

%.tab.h: %.tab.c
%.tab.c %.tab.h: %.y
	$(BISON) -v $<
#endif

clean-local:
	rm -f *.s *.i

%.sh: %.in
	@echo "Creating $@"
	@sed \
		-e 's|@top_builddir\@|$(top_builddir)|g' \
		-e 's|@camlibdir\@|$(camlibdir)|g' \
		< "$<" > "$@"
	@chmod +x $@

check-ddb.sh: check-ddb.in $(top_builddir)/packaging/generic/print-camera-list test-ddb

$(top_builddir)/packaging/generic/print-camera-list:
	cd $(top_builddir)/packaging/generic && $(MAKE) print-camera-list
