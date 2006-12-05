EXTRA_DIST = setup.py.in gphoto2.pyx README INSTALL gptest.py test-gp.py

BUILT_SOURCES = gphoto2.c

PYTHON = python

# CAUTION: The dependencies are still a little broken.
#          Better run "make clean" before "make all" and "make check".

gphoto2.c: gphoto2.pyx
	pyrexc \
		-I $(top_srcdir)/libgphoto2_port/libgphoto2_port \
		-I $(top_srcdir)/libgphoto2 \
		-I $(top_builddir)/libgphoto2 \
		-o $@ \
		$<

clean-local:
	rm -rf test-install
	rm -f $(BUILT_SOURCES)

all-local:
	CFLAGS="$(AM_CPPFLAGS) $(AM_CFLAGS) $(CPPFLAGS) $(CFLAGS)" $(PYTHON) setup.py build
	$(PYTHON) setup.py install -O1 --skip-build --root "$(PWD)/test-install"

blubb-install-data-local: setup.py
	@if test "x$(DESTDIR)" = "x"; then \
		echo "$(PYTHON) setup.py install -O1 --skip-build"; \
		$(PYTHON) setup.py install -O1 --skip-build; \
	else \
		echo "$(PYTHON) setup.py install -O1 --skip-build --root $(DESTDIR)"; \
		$(PYTHON) setup.py install -O1 --skip-build --root "$(DESTDIR)"; \
	fi

TESTS = test-gp.py
