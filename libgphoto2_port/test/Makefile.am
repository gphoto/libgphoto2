TESTS =
INSTALL_TESTS =
check_PROGRAMS =
noinst_PROGRAMS =

# When we build all our iolibs in one directory, we can run our checks
# with IOLIBS set to the camlib build directory.
TESTS_ENVIRONMENT = env \
	IOLIBS="$(top_builddir)"

# After installation, this will be IOLIBS = $(DESTDIR)$(iolibdir)
INSTALL_TESTS_ENVIRONMENT = env \
	IOLIBS="$(DESTDIR)$(iolibdir)" \
	LD_LIBRARY_PATH="$(DESTDIR)$(libdir)$${LD_LIBRARY_PATH+:$${LD_LIBRARY_PATH}}"

AM_CPPFLAGS += -I$(top_srcdir)

noinst_PROGRAMS += test-gp-port test-port-list
test_gp_port_CPPFLAGS = $(AM_CPPFLAGS) $(LTDLINCL) $(CPPFLAGS)
test_gp_port_SOURCE = test-gp-port.c
test_gp_port_LDFLAGS = $(top_builddir)/libgphoto2_port/libgphoto2_port.la $(LIBLTDL) $(INTLLIBS)

TESTS += test-port-list
INSTALL_TESTS += test-port-list
check_PROGRAMS += test-port-list
test_port_list_CPPFLAGS = $(AM_CPPFLAGS) $(LTDLINCL) $(CPPFLAGS)
test_port_list_SOURCE = test-gp-port.c
test_port_list_LDFLAGS = \
	$(top_builddir)/libgphoto2_port/libgphoto2_port.la \
	$(LIBLTDL) \
	$(INTLLIBS)

include $(top_srcdir)/installcheck.mk
