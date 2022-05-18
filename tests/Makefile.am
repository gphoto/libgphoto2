########################################################################
# Housekeeping
########################################################################

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
AM_CPPFLAGS += -I$(top_srcdir) -I$(top_builddir)  -I$(top_srcdir)/libgphoto2_port


# Test i18n
check_PROGRAMS     += test-i18n
test_i18n_SOURCES   = test-i18n.c
test_i18n_CPPFLAGS  = $(AM_CPPFLAGS)
test_i18n_CFLAGS    = $(AM_CFLAGS)
test_i18n_LDADD     = $(INTLLIBS)


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


TESTS          += test-init-localedir
check_PROGRAMS += test-init-localedir
test_init_localedir_LDADD =
test_init_localedir_LDADD += $(top_builddir)/libgphoto2/libgphoto2.la
test_init_localedir_LDADD += $(top_builddir)/libgphoto2_port/libgphoto2_port/libgphoto2_port.la
test_init_localedir_LDADD += $(LIBLTDL)
test_init_localedir_LDADD += $(LIBEXIF_LIBS)
test_init_localedir_LDADD += $(INTLLIBS)


########################################################################
# Test pedantic compilation for multiple language standard
########################################################################

generic_pedantic_cppflags   =
generic_pedantic_cppflags  += -I$(top_srcdir)/libgphoto2_port
generic_pedantic_cppflags  += -I$(top_srcdir)

generic_pedantic_cflags     =

generic_pedantic_cxxflags   =

if GP_HAVE_PEDANTIC_FLAGS_C90
TESTS                      += test-pedantic-c90
check_PROGRAMS             += test-pedantic-c90
endif
EXTRA_PROGRAMS             += test-pedantic-c90
test_pedantic_c90_SOURCES   = test-pedantic-compilation.c
test_pedantic_c90_CPPFLAGS  = $(generic_pedantic_cppflags)
test_pedantic_c90_CFLAGS    = $(GP_PEDANTIC_CFLAGS_C90)
test_pedantic_c90_CFLAGS   += $(generic_pedantic_cflags)
test_pedantic_c90_LDADD     =
test_pedantic_c90_LDFLAGS   =

if GP_HAVE_PEDANTIC_FLAGS_C99
TESTS                      += test-pedantic-c99
check_PROGRAMS             += test-pedantic-c99
endif
EXTRA_PROGRAMS             += test-pedantic-c99
test_pedantic_c99_SOURCES   = test-pedantic-compilation.c
test_pedantic_c99_CPPFLAGS  = $(generic_pedantic_cppflags)
test_pedantic_c99_CFLAGS    = $(GP_PEDANTIC_CFLAGS_C99)
test_pedantic_c99_CFLAGS   += $(generic_pedantic_cflags)
test_pedantic_c99_LDADD     =
test_pedantic_c99_LDFLAGS   =

if GP_HAVE_PEDANTIC_FLAGS_C11
TESTS                      += test-pedantic-c11
check_PROGRAMS             += test-pedantic-c11
endif
EXTRA_PROGRAMS             += test-pedantic-c11
test_pedantic_c11_SOURCES   = test-pedantic-compilation.c
test_pedantic_c11_CPPFLAGS  = $(generic_pedantic_cppflags)
test_pedantic_c11_CFLAGS    = $(GP_PEDANTIC_CFLAGS_C11)
test_pedantic_c11_CFLAGS   += $(generic_pedantic_cflags)
test_pedantic_c11_LDADD     =
test_pedantic_c11_LDFLAGS   =

if GP_HAVE_PEDANTIC_FLAGS_C2X
TESTS                      += test-pedantic-c2x
check_PROGRAMS             += test-pedantic-c2x
endif
EXTRA_PROGRAMS             += test-pedantic-c2x
test_pedantic_c2x_SOURCES   = test-pedantic-compilation.c
test_pedantic_c2x_CPPFLAGS  = $(generic_pedantic_cppflags)
test_pedantic_c2x_CFLAGS    = $(GP_PEDANTIC_CFLAGS_C2X)
test_pedantic_c2x_CFLAGS   += $(generic_pedantic_cflags)
test_pedantic_c2x_LDADD     =
test_pedantic_c2x_LDFLAGS   =

if HAVE_CXX
if GP_HAVE_PEDANTIC_FLAGS_CXX98
TESTS                        += test-pedantic-cxx98
check_PROGRAMS               += test-pedantic-cxx98
endif
endif
EXTRA_PROGRAMS               += test-pedantic-cxx98
test_pedantic_cxx98_SOURCES   = test-pedantic-compilation.cxx
test_pedantic_cxx98_CPPFLAGS  = $(generic_pedantic_cppflags)
test_pedantic_cxx98_CXXFLAGS  = $(GP_PEDANTIC_CXXFLAGS_CXX98)
test_pedantic_cxx98_CXXFLAGS += $(generic_pedantic_cxxflags)
test_pedantic_cxx98_LDADD     =
test_pedantic_cxx98_LDFLAGS   =

if HAVE_CXX
if GP_HAVE_PEDANTIC_FLAGS_CXX11
TESTS                        += test-pedantic-cxx11
check_PROGRAMS               += test-pedantic-cxx11
endif
endif
EXTRA_PROGRAMS               += test-pedantic-cxx11
test_pedantic_cxx11_SOURCES   = test-pedantic-compilation.cxx
test_pedantic_cxx11_CPPFLAGS  = $(generic_pedantic_cppflags)
test_pedantic_cxx11_CXXFLAGS  = $(GP_PEDANTIC_CXXFLAGS_CXX11)
test_pedantic_cxx11_CXXFLAGS += $(generic_pedantic_cxxflags)
test_pedantic_cxx11_LDADD     =
test_pedantic_cxx11_LDFLAGS   =

if HAVE_CXX
if GP_HAVE_PEDANTIC_FLAGS_CXX14
TESTS                        += test-pedantic-cxx14
check_PROGRAMS               += test-pedantic-cxx14
endif
endif
EXTRA_PROGRAMS               += test-pedantic-cxx14
test_pedantic_cxx14_SOURCES   = test-pedantic-compilation.cxx
test_pedantic_cxx14_CPPFLAGS  = $(generic_pedantic_cppflags)
test_pedantic_cxx14_CXXFLAGS  = $(GP_PEDANTIC_CXXFLAGS_CXX14)
test_pedantic_cxx14_CXXFLAGS += $(generic_pedantic_cxxflags)
test_pedantic_cxx14_LDADD     =
test_pedantic_cxx14_LDFLAGS   =

if HAVE_CXX
if GP_HAVE_PEDANTIC_FLAGS_CXX17
TESTS                        += test-pedantic-cxx17
check_PROGRAMS               += test-pedantic-cxx17
endif
endif
EXTRA_PROGRAMS               += test-pedantic-cxx17
test_pedantic_cxx17_SOURCES   = test-pedantic-compilation.cxx
test_pedantic_cxx17_CPPFLAGS  = $(generic_pedantic_cppflags)
test_pedantic_cxx17_CXXFLAGS  = $(GP_PEDANTIC_CXXFLAGS_CXX17)
test_pedantic_cxx17_CXXFLAGS += $(generic_pedantic_cxxflags)
test_pedantic_cxx17_LDADD     =
test_pedantic_cxx17_LDFLAGS   =

if HAVE_CXX
if GP_HAVE_PEDANTIC_FLAGS_CXX20
TESTS                        += test-pedantic-cxx20
check_PROGRAMS               += test-pedantic-cxx20
endif
endif
EXTRA_PROGRAMS               += test-pedantic-cxx20
test_pedantic_cxx20_SOURCES   = test-pedantic-compilation.cxx
test_pedantic_cxx20_CPPFLAGS  = $(generic_pedantic_cppflags)
test_pedantic_cxx20_CXXFLAGS  = $(GP_PEDANTIC_CXXFLAGS_CXX20)
test_pedantic_cxx20_CXXFLAGS += $(generic_pedantic_cxxflags)
test_pedantic_cxx20_LDADD     =
test_pedantic_cxx20_LDFLAGS   =


########################################################################
# Implement the checks for the installed library
########################################################################

include $(top_srcdir)/installcheck.mk
