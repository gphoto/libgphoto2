TESTS =
INSTALL_TESTS =
check_PROGRAMS =
noinst_PROGRAMS =

# When we build all our iolibs in one directory, we can run our checks
# with IOLIBS set to the iolib build directory.
TESTS_ENVIRONMENT = env \
	IOLIBS="$(top_builddir)"

# After installation, this will be IOLIBS = $(DESTDIR)$(iolibdir)
INSTALL_TESTS_ENVIRONMENT = env \
	IOLIBS="$(DESTDIR)$(iolibdir)" \
	LD_LIBRARY_PATH="$(DESTDIR)$(libdir)$${LD_LIBRARY_PATH+:$${LD_LIBRARY_PATH}}"

AM_CPPFLAGS += -I$(top_srcdir)

noinst_PROGRAMS += test-gp-port
test_gp_port_CPPFLAGS = $(AM_CPPFLAGS) $(LTDLINCL) $(CPPFLAGS)
test_gp_port_SOURCES = test-gp-port.c
test_gp_port_LDADD = $(top_builddir)/libgphoto2_port/libgphoto2_port.la
test_gp_port_LDADD += $(LIBLTDL) $(INTLLIBS)

TESTS += test-port-list
INSTALL_TESTS += test-port-list
check_PROGRAMS += test-port-list
noinst_PROGRAMS += test-port-list
test_port_list_CPPFLAGS = $(AM_CPPFLAGS) $(LTDLINCL) $(CPPFLAGS)
test_port_list_SOURCES = test-port-list.c
test_port_list_LDADD = $(top_builddir)/libgphoto2_port/libgphoto2_port.la
test_port_list_LDADD += $(LIBLTDL) $(INTLLIBS)

include $(top_srcdir)/installcheck.mk
