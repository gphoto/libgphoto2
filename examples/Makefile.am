EXTRA_DIST = README
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


noinst_PROGRAMS += \
	sample-autodetect sample-multi-detect \
	lunkwill-canon-capture  \
	sample-owner \
	sample-preview \
	sample-tether \
	best-iso \
	sample-capture sample-trigger-capture sample-justfocus

sample_autodetect_SOURCES = sample-autodetect.c samples.h context.c config.c
sample_owner_SOURCES = sample-owner.c samples.h context.c config.c
sample_multi_detect_SOURCES = sample-multi-detect.c samples.h autodetect.c context.c config.c
lunkwill_canon_capture_SOURCES = lunkwill-canon-capture.c context.c samples.h config.c
sample_capture_SOURCES = sample-capture.c context.c samples.h config.c focus.c
sample_trigger_capture_SOURCES = sample-trigger-capture.c context.c samples.h config.c
sample_preview_SOURCES = preview.c samples.h context.c config.c focus.c
sample_tether_SOURCES = samples.h sample-tether.c context.c config.c
best_iso_SOURCES = best-iso.c
sample_justfocus_SOURCES = samples.h sample-justfocus.c context.c focus.c

LDADD = $(top_builddir)/libgphoto2/libgphoto2.la \
	$(top_builddir)/libgphoto2_port/libgphoto2_port/libgphoto2_port.la \
	$(LIBLTDL) \
	$(LIBEXIF_LIBS) \
	$(INTLLIBS)

#noinst_PROGRAMS += test-filesys
#test_filesys_SOURCE = test-filesys.c
#test_filesys_LDADD = \
#	$(top_builddir)/libgphoto2/libgphoto2.la \
#	$(top_builddir)/libgphoto2_port/libgphoto2_port/libgphoto2_port.la \
#	$(LIBLTDL) \
#	$(LIBEXIF_LIBS) \
#	$(INTLLIBS)



#TESTS += test-camera-list
#INSTALL_TESTS += test-camera-list
#check_PROGRAMS += test-camera-list
#test_camera_list_SOURCE = test-camera-list.c
#test_camera_list_LDADD = \
#	$(top_builddir)/libgphoto2/libgphoto2.la \
#	$(top_builddir)/libgphoto2_port/libgphoto2_port/libgphoto2_port.la \
#	$(LIBLTDL) \
#	$(LIBEXIF_LIBS) \
#	$(INTLLIBS)


#if HAVE_GCC
#PEDANTIC_CFLAGS = -std=c99 -pedantic-errors -W -Wall -Wextra -Werror
#PEDANTIC_CXXFLAGS = -std=c++98 -pedantic-errors -W -Wall -Wextra -Werror
#else
#PEDANTIC_CFLAGS = 
#PEDANTIC_CXXFLAGS = 
#endif
#
#TESTS += test-pedantic-c
#check_PROGRAMS += test-pedantic-c
#test_pedantic_c_SOURCES = test-pedantic-compilation.c
#test_pedantic_c_CPPFLAGS = \
#	$(AM_CPPFLAGS) $(CPPFLAGS) \
#	-U_GPHOTO2_INTERNAL_CODE
#test_pedantic_c_CFLAGS = \
#	$(AM_CFLAGS) $(PEDANTIC_CFLAGS) $(CFLAGS) \
#	-U_GPHOTO2_INTERNAL_CODE
#test_pedantic_c_LDADD = \
#	$(top_builddir)/libgphoto2/libgphoto2.la \
#	$(top_builddir)/libgphoto2_port/libgphoto2_port/libgphoto2_port.la \
#	$(LIBLTDL) \
#	$(LIBEXIF_LIBS) \
#	$(INTLLIBS)


#if HAVE_CXX
#check_PROGRAMS += test-pedantic-cxx
#TESTS += test-pedantic-cxx
#else
#EXTRA_PROGRAMS += test-pedantic-cxx
#endif
#test_pedantic_cxx_SOURCES = test-pedantic-compilation.cxx
#test_pedantic_cxx_CPPFLAGS = \
#	$(AM_CPPFLAGS) $(CPPFLAGS) \
#	-U_GPHOTO2_INTERNAL_CODE
#test_pedantic_cxx_CXXFLAGS = \
#	$(AM_CXXFLAGS) $(PEDANTIC_CXXFLAGS) $(CXXFLAGS) \
#	-U_GPHOTO2_INTERNAL_CODE
#test_pedantic_cxx_LDADD = \
#	$(top_builddir)/libgphoto2/libgphoto2.la \
#	$(top_builddir)/libgphoto2_port/libgphoto2_port/libgphoto2_port.la \
#	$(LIBLTDL) \
#	$(LIBEXIF_LIBS) \
#	$(INTLLIBS)

include $(top_srcdir)/installcheck.mk
