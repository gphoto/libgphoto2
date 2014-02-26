SUBDIRS = ddb

EXTRA_DIST =
EXTRA_PROGRAMS =
check_PROGRAMS =
check_SCRIPTS =
TESTS =
INSTALL_TESTS =
noinst_PROGRAMS =


# Now that we build all the camlibs in one directory, we can run our checks
# with CAMLIBS set to the camlib build directory.
TESTS_ENVIRONMENT = env \
	CAMLIBS="$(top_builddir)/camlibs"

# After installation, this will be CAMLIBS = $(DESTDIR)$(camlibdir)
INSTALL_TESTS_ENVIRONMENT = env \
	CAMLIBS="$(DESTDIR)$(camlibdir)" \
	LD_LIBRARY_PATH="$(DESTDIR)$(libdir)$${LD_LIBRARY_PATH+:$${LD_LIBRARY_PATH}}"

AM_CPPFLAGS += -I$(top_srcdir) -I$(top_builddir)  -I$(top_srcdir)/libgphoto2_port -I$(top_srcdir)/libgphoto2 -I$(top_builddir)/libgphoto2

CLEANFILES = $(check_SCRIPTS)


TESTS += test-endian
check_PROGRAMS += test-endian
test_endian_SOURCE = test-endian.c
test_endian_LDADD = \
	$(top_builddir)/libgphoto2/libgphoto2.la \
	$(top_builddir)/libgphoto2_port/libgphoto2_port/libgphoto2_port.la \
	$(LIBLTDL) \
	$(LIBEXIF_LIBS) \
	$(INTLLIBS)

noinst_PROGRAMS += test-gphoto2
test_gphoto2_SOURCE = test-gphoto2.c
test_gphoto2_LDADD = \
	$(top_builddir)/libgphoto2/libgphoto2.la \
	$(top_builddir)/libgphoto2_port/libgphoto2_port/libgphoto2_port.la \
	$(LIBLTDL) \
	$(LIBEXIF_LIBS) \
	$(INTLLIBS)

noinst_PROGRAMS += test-filesys
test_filesys_SOURCE = test-filesys.c
test_filesys_LDADD = \
	$(top_builddir)/libgphoto2/libgphoto2.la \
	$(top_builddir)/libgphoto2_port/libgphoto2_port/libgphoto2_port.la \
	$(LIBLTDL) \
	$(LIBEXIF_LIBS) \
	$(INTLLIBS)



TESTS += test-camera-list
INSTALL_TESTS += test-camera-list
check_PROGRAMS += test-camera-list
test_camera_list_SOURCE = test-camera-list.c
test_camera_list_LDADD = \
	$(top_builddir)/libgphoto2/libgphoto2.la \
	$(top_builddir)/libgphoto2_port/libgphoto2_port/libgphoto2_port.la \
	$(LIBLTDL) \
	$(LIBEXIF_LIBS) \
	$(INTLLIBS)


if HAVE_GCC
PEDANTIC_CFLAGS = -std=c99 -pedantic-errors -W -Wall -Wextra -Werror
PEDANTIC_CXXFLAGS = -std=c++98 -pedantic-errors -W -Wall -Wextra -Werror
else
PEDANTIC_CFLAGS = 
PEDANTIC_CXXFLAGS = 
endif

TESTS += test-pedantic-c
check_PROGRAMS += test-pedantic-c
test_pedantic_c_SOURCES = test-pedantic-compilation.c
test_pedantic_c_CPPFLAGS = \
	$(AM_CPPFLAGS) $(CPPFLAGS) \
	-U_GPHOTO2_INTERNAL_CODE
test_pedantic_c_CFLAGS = \
	$(AM_CFLAGS) $(PEDANTIC_CFLAGS) $(CFLAGS) \
	-U_GPHOTO2_INTERNAL_CODE
test_pedantic_c_LDADD = \
	$(top_builddir)/libgphoto2/libgphoto2.la \
	$(top_builddir)/libgphoto2_port/libgphoto2_port/libgphoto2_port.la \
	$(LIBLTDL) \
	$(LIBEXIF_LIBS) \
	$(INTLLIBS)


if HAVE_CXX
check_PROGRAMS += test-pedantic-cxx
TESTS += test-pedantic-cxx
else
EXTRA_PROGRAMS += test-pedantic-cxx
endif
test_pedantic_cxx_SOURCES = test-pedantic-compilation.cxx
test_pedantic_cxx_CPPFLAGS = \
	$(AM_CPPFLAGS) $(CPPFLAGS) \
	-U_GPHOTO2_INTERNAL_CODE
test_pedantic_cxx_CXXFLAGS = \
	$(AM_CXXFLAGS) $(PEDANTIC_CXXFLAGS) $(CXXFLAGS) \
	-U_GPHOTO2_INTERNAL_CODE
test_pedantic_cxx_LDADD = \
	$(top_builddir)/libgphoto2/libgphoto2.la \
	$(top_builddir)/libgphoto2_port/libgphoto2_port/libgphoto2_port.la \
	$(LIBLTDL) \
	$(LIBEXIF_LIBS) \
	$(INTLLIBS)

include $(top_srcdir)/installcheck.mk
