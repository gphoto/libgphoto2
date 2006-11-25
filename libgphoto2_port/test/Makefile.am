noinst_PROGRAMS = test-gp-port

test_gp_port_CPPFLAGS = \
	$(AM_CPPFLAGS) $(CPPFLAGS) \
	$(LTDLINCL)
test_gp_port_SOURCE = test-gp-port.c
test_gp_port_LDFLAGS = $(top_builddir)/libgphoto2_port/libgphoto2_port.la $(LIBLTDL) $(INTLLIBS)
