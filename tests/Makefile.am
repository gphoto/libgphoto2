SUBDIRS = ddb

check_PROGRAMS = test-camera-list

check_SCRIPTS = \
	check-camera-list.sh

EXTRA_DIST = \
	check-camera-list.in

# TESTS = $(check_SCRIPTS)

# FIXME: Only run this if not cross-compiling
installcheck-local: test-camera-list
	@export CAMLIBS="$(DESTDIR)$(camlibdir)"; \
	total=0; failed=0; success=0; \
	echo "Running \"installcheck\" test cases..."; \
	for script in test-camera-list; do \
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

CLEANFILES = $(check_SCRIPTS)

noinst_PROGRAMS =	\
	test-gphoto2	\
	test-filesys

test_gphoto2_SOURCE = test-gphoto2.c
test_gphoto2_LDADD = \
	$(top_builddir)/libgphoto2/libgphoto2.la \
	$(top_builddir)/libgphoto2_port/libgphoto2_port/libgphoto2_port.la \
	$(LIBEXIF_LIBS) \
	$(INTLLIBS)

test_filesys_SOURCE = test-filesys.c
test_filesys_LDADD = \
	$(top_builddir)/libgphoto2/libgphoto2.la \
	$(top_builddir)/libgphoto2_port/libgphoto2_port/libgphoto2_port.la \
	$(LIBEXIF_LIBS) \
	$(INTLLIBS)

test_camera_list_SOURCE = test-camera-list.c
test_camera_list_LDADD = \
	$(top_builddir)/libgphoto2/libgphoto2.la \
	$(top_builddir)/libgphoto2_port/libgphoto2_port/libgphoto2_port.la \
	$(LIBEXIF_LIBS) \
	$(INTLLIBS)

clean-local:
	rm -rf _inst
	rm -f _camlibs

.in.sh:
	@echo "Creating $@"
	@sed \
		-e 's|@top_builddir\@|$(top_builddir)|g' \
		-e 's|@camlibdir\@|$(camlibdir)|g' \
		-e 's|@libdir\@|$(libdir)|g' \
		-e 's|@DESTDIR\@|$(DESTDIR)|g' \
		-e 's|@MAKE\@|$(MAKE)|g' \
		< "$<" > "$@"
	@chmod +x $@

