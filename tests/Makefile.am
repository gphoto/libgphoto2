SUBDIRS = ddb

check_PROGRAMS = \
	test-camera-list \
	test-pedantic-c

if HAVE_CXX
check_PROGRAMS += test-pedantic-cxx
endif

check_SCRIPTS = \
	check-camera-list.sh

EXTRA_DIST = \
	check-camera-list.sh.in

TESTS = test-pedantic-c test-pedantic-cxx

# These tests must run after installation because they need the
# camlibs installed.
INSTALL_TESTS = check-camera-list.sh

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

if HAVE_GCC
PEDANTIC_CFLAGS = -std=c99 -pedantic-errors -W -Wall -Wextra -Werror
PEDANTIC_CXXFLAGS = -std=c++98 -pedantic-errors -W -Wall -Wextra -Werror
else
PEDANTIC_CFLAGS = 
PEDANTIC_CXXFLAGS = 
endif

test_pedantic_c_SOURCES = test-pedantic-compilation.c
test_pedantic_c_CPPFLAGS = \
	$(AM_CPPFLAGS) $(CPPFLAGS) \
	-U_GPHOTO2_INTERNAL_CODE
test_pedantic_c_CFLAGS = \
	$(PEDANTIC_CFLAGS) $(AM_CFLAGS) $(CFLAGS) \
	-U_GPHOTO2_INTERNAL_CODE
test_pedantic_c_LDADD = \
	$(top_builddir)/libgphoto2/libgphoto2.la \
	$(top_builddir)/libgphoto2_port/libgphoto2_port/libgphoto2_port.la \
	$(LIBEXIF_LIBS) \
	$(INTLLIBS)

test_pedantic_cxx_SOURCES = test-pedantic-compilation.cxx
test_pedantic_cxx_CPPFLAGS = \
	$(AM_CPPFLAGS) $(CPPFLAGS) \
	-U_GPHOTO2_INTERNAL_CODE
test_pedantic_cxx_CXXFLAGS = \
	$(PEDANTIC_CXXFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS) \
	-U_GPHOTO2_INTERNAL_CODE
test_pedantic_cxx_LDADD = \
	$(top_builddir)/libgphoto2/libgphoto2.la \
	$(top_builddir)/libgphoto2_port/libgphoto2_port/libgphoto2_port.la \
	$(LIBEXIF_LIBS) \
	$(INTLLIBS)

include $(top_srcdir)/installcheck.mk
