EXTRA_DIST = setup.py.in gphoto2.pyx README INSTALL gptest.py test-gp.py

# CAUTION: The dependencies are still a little broken.
#          Better run "make clean" before "make all" and "make check".

if HAVE_PYTHON

BUILT_SOURCES = gphoto2.c

PYTHON_LIB = test-install$(python_sitearchdir)/gphoto2.so

gphoto2.c: gphoto2.pyx
	pyrexc \
		-I $(top_srcdir)/libgphoto2_port/libgphoto2_port \
		-I $(top_srcdir)/libgphoto2 \
		-I $(top_builddir)/libgphoto2 \
		-o $@ \
		$<

clean-local:
	rm -rf test-install build
	rm -f $(BUILT_SOURCES)

all-local: $(PYTHON_LIB)

$(PYTHON_LIB): gphoto2.c
	CFLAGS="$(AM_CPPFLAGS) $(AM_CFLAGS) $(CPPFLAGS) $(CFLAGS)" $(PYTHON) setup.py build
	$(PYTHON) setup.py install -O1 --skip-build --root "$(PWD)/test-install"
	touch $@

# Ignore errors to help 'make distcheck'
install-data-local: $(PYTHON_LIB)
	-install -m 0755 -d $(DESTDIR)$(python_sitearchdir)
	-install -m 0755 $< $(DESTDIR)$(python_sitearchdir)/

uninstall-local:
	rm -f $(DESTDIR)$(python_sitearchdir)/$$(basename "$(PYTHON_LIB)")

# Unused
blubb-install-data-local: setup.py
	@if test "x$(DESTDIR)" = "x"; then \
		echo "$(PYTHON) setup.py install -O1 --skip-build"; \
		$(PYTHON) setup.py install -O1 --skip-build; \
	else \
		echo "$(PYTHON) setup.py install -O1 --skip-build --root $(DESTDIR)"; \
		$(PYTHON) setup.py install -O1 --skip-build --root "$(DESTDIR)"; \
	fi

INSTALL_TESTS_ENVIRONMENT = env \
	python_sitearchdir=$(PWD)/test-install$(python_sitearchdir) \
	LD_LIBRARY_PATH=$(DESTDIR)/$(libdir):$(LD_LIBRARY_PATH)

INSTALL_TESTS = test-gp.py
INSTALLCHECK_DEPS = $(PYTHON_LIB)

# Breaks "make distdir" due to automake expecting all install dirs to
# depend on $prefix :-(
# So we use those local install hooks.
# python_sitearch_DATA = test-install$(python_sitearchdir)/gphoto2.so

endif

include $(top_srcdir)/installcheck.mk
