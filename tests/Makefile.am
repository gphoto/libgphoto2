########################################################################
# Housekeeping
########################################################################

SUBDIRS = ddb

EXTRA_PROGRAMS =
check_PROGRAMS =
TESTS =
INSTALL_TESTS =
noinst_PROGRAMS =


########################################################################
# Set up test environment
########################################################################

# Now that we build all the camlibs in one directory, we can run our checks
# with CAMLIBS set to the camlib build directory.
TESTS_ENVIRONMENT = env \
	CAMLIBS="$(top_builddir)/camlibs"

# After installation, this will be CAMLIBS = $(DESTDIR)$(camlibdir)
INSTALL_TESTS_ENVIRONMENT = env \
	CAMLIBS="$(DESTDIR)$(camlibdir)" \
	LD_LIBRARY_PATH="$(DESTDIR)$(libdir)$${LD_LIBRARY_PATH+:$${LD_LIBRARY_PATH}}"


########################################################################
# Define test programs
########################################################################


# Default CPPFLAGS to use
AM_CPPFLAGS += -I$(top_srcdir) -I$(top_builddir)  -I$(top_srcdir)/libgphoto2_port -I$(top_srcdir)/libgphoto2 -I$(top_builddir)/libgphoto2


# Test endianness conversions
TESTS              += test-endian
check_PROGRAMS     += test-endian
test_endian_SOURCES = test-endian.c
test_endian_LDADD   = \
	$(top_builddir)/libgphoto2/libgphoto2.la \
	$(top_builddir)/libgphoto2_port/libgphoto2_port/libgphoto2_port.la \
	$(LIBLTDL) \
	$(LIBEXIF_LIBS) \
	$(INTLLIBS)


# Print the libgphoto2 version information for this libgphoto2 build
noinst_PROGRAMS                 += print-libgphoto2-version
print_libgphoto2_version_SOURCES = print-libgphoto2-version.c
print_libgphoto2_version_LDADD   =
print_libgphoto2_version_LDADD  += $(top_builddir)/libgphoto2/libgphoto2.la
print_libgphoto2_version_LDADD  += $(top_builddir)/libgphoto2_port/libgphoto2_port/libgphoto2_port.la
print_libgphoto2_version_LDADD  += $(LIBLTDL)
print_libgphoto2_version_LDADD  += $(LIBEXIF_LIBS)
print_libgphoto2_version_LDADD  += $(INTLLIBS)


# Test basic libgphoto2 functionality
noinst_PROGRAMS     += test-gphoto2
test_gphoto2_SOURCES = test-gphoto2.c
test_gphoto2_LDADD   = \
	$(top_builddir)/libgphoto2/libgphoto2.la \
	$(top_builddir)/libgphoto2_port/libgphoto2_port/libgphoto2_port.la \
	$(LIBLTDL) \
	$(LIBEXIF_LIBS) \
	$(INTLLIBS)


# Test gp_filesystem_* functions
noinst_PROGRAMS    += test-filesys
test_filesys_SOURCES = test-filesys.c
test_filesys_LDADD   = \
	$(top_builddir)/libgphoto2/libgphoto2.la \
	$(top_builddir)/libgphoto2_port/libgphoto2_port/libgphoto2_port.la \
	$(LIBLTDL) \
	$(LIBEXIF_LIBS) \
	$(INTLLIBS)


# Print a list of all cameras supported by this build of libgphoto2
TESTS          += test-camera-list
INSTALL_TESTS  += test-camera-list
check_PROGRAMS         += test-camera-list
test_camera_list_SOURCES = test-camera-list.c
test_camera_list_LDADD   = \
	$(top_builddir)/libgphoto2/libgphoto2.la \
	$(top_builddir)/libgphoto2_port/libgphoto2_port/libgphoto2_port.la \
	$(LIBLTDL) \
	$(LIBEXIF_LIBS) \
	$(INTLLIBS)


########################################################################
# Old pedantic compilation test
########################################################################

if HAVE_GCC
PEDANTIC_CFLAGS   = -std=c99 -pedantic-errors -W -Wall -Wextra -Werror
PEDANTIC_CXXFLAGS = -std=c++98 -pedantic-errors -W -Wall -Wextra -Werror
else
PEDANTIC_CFLAGS   =
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


########################################################################
# Test pendantic compilation for multiple language standard
########################################################################

generic_pedantic_cppflags =
generic_pedantic_cppflags += -I$(top_srcdir)/libgphoto2_port
generic_pedantic_cppflags += -I$(top_srcdir)

if GP_HAVE_PEDANTIC_FLAGS_C90
TESTS                     += test-pedantic-c90
check_PROGRAMS            += test-pedantic-c90
test_pedantic_c90_SOURCES  = test-pedantic-compilation.c
test_pedantic_c90_CPPFLAGS = $(generic_pedantic_cppflags)
test_pedantic_c90_CFLAGS   = $(GP_PEDANTIC_CFLAGS_C90)
test_pedantic_c90_LDADD    =
test_pedantic_c90_LDFLAGS  =
endif

if GP_HAVE_PEDANTIC_FLAGS_C99
TESTS                     += test-pedantic-c99
check_PROGRAMS            += test-pedantic-c99
test_pedantic_c99_SOURCES  = test-pedantic-compilation.c
test_pedantic_c99_CPPFLAGS = $(generic_pedantic_cppflags)
test_pedantic_c99_CFLAGS   = $(GP_PEDANTIC_CFLAGS_C99)
test_pedantic_c99_LDADD    =
test_pedantic_c99_LDFLAGS  =
endif

if GP_HAVE_PEDANTIC_FLAGS_C11
TESTS                     += test-pedantic-c11
check_PROGRAMS            += test-pedantic-c11
test_pedantic_c11_SOURCES  = test-pedantic-compilation.c
test_pedantic_c11_CPPFLAGS = $(generic_pedantic_cppflags)
test_pedantic_c11_CFLAGS   = $(GP_PEDANTIC_CFLAGS_C11)
test_pedantic_c11_LDADD    =
test_pedantic_c11_LDFLAGS  =
endif

if GP_HAVE_PEDANTIC_FLAGS_CXX98
if HAVE_CXX
TESTS                       += test-pedantic-cxx98
check_PROGRAMS              += test-pedantic-cxx98
else
EXTRA_PROGRAMS              += test-pedantic-cxx98
endif
test_pedantic_cxx98_SOURCES  = test-pedantic-compilation.cxx
test_pedantic_cxx98_CPPFLAGS = $(generic_pedantic_cppflags)
test_pedantic_cxx98_CXXFLAGS = $(GP_PEDANTIC_CXXFLAGS_CXX98)
test_pedantic_cxx98_LDADD    =
test_pedantic_cxx98_LDFLAGS  =
endif

if GP_HAVE_PEDANTIC_FLAGS_CXX11
if HAVE_CXX
TESTS                       += test-pedantic-cxx11
check_PROGRAMS              += test-pedantic-cxx11
else
EXTRA_PROGRAMS              += test-pedantic-cxx11
endif
test_pedantic_cxx11_SOURCES  = test-pedantic-compilation.cxx
test_pedantic_cxx11_CPPFLAGS = $(generic_pedantic_cppflags)
test_pedantic_cxx11_CXXFLAGS = $(GP_PEDANTIC_CXXFLAGS_CXX11)
test_pedantic_cxx11_LDADD    =
test_pedantic_cxx11_LDFLAGS  =
endif

if GP_HAVE_PEDANTIC_FLAGS_CXX14
if HAVE_CXX
TESTS                       += test-pedantic-cxx14
check_PROGRAMS              += test-pedantic-cxx14
else
EXTRA_PROGRAMS              += test-pedantic-cxx14
endif
test_pedantic_cxx14_SOURCES  = test-pedantic-compilation.cxx
test_pedantic_cxx14_CPPFLAGS = $(generic_pedantic_cppflags)
test_pedantic_cxx14_CXXFLAGS = $(GP_PEDANTIC_CXXFLAGS_CXX14)
test_pedantic_cxx14_LDADD    =
test_pedantic_cxx14_LDFLAGS  =
endif

if GP_HAVE_PEDANTIC_FLAGS_CXX17
if HAVE_CXX
TESTS                       += test-pedantic-cxx17
check_PROGRAMS              += test-pedantic-cxx17
else
EXTRA_PROGRAMS              += test-pedantic-cxx17
endif
test_pedantic_cxx17_SOURCES  = test-pedantic-compilation.cxx
test_pedantic_cxx17_CPPFLAGS = $(generic_pedantic_cppflags)
test_pedantic_cxx17_CXXFLAGS = $(GP_PEDANTIC_CXXFLAGS_CXX17)
test_pedantic_cxx17_LDADD    =
test_pedantic_cxx17_LDFLAGS  =
endif


########################################################################
# Implement the checks for the installed library
########################################################################

include $(top_srcdir)/installcheck.mk
