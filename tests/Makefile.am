
INCLUDES =						\
	-I$(top_srcdir)/libgphoto2			\
	-I$(top_srcdir)/libgphoto2_port/libgphoto2_port

check_PROGRAMS = test-camera-list

check_SCRIPTS = \
	check-camera-list.sh

EXTRA_DIST = \
	check-camera-list.in

TESTS = $(check_SCRIPTS)

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

clean-local:
	rm -rf _inst

%.sh: %.in
	@echo "Creating $@"
	@sed \
		-e 's|@top_builddir\@|$(top_builddir)|g' \
		-e 's|@camlibdir\@|$(camlibdir)|g' \
		-e 's|@DESTDIR\@|$(DESTDIR)|g' \
		-e 's|@MAKE\@|$(MAKE)|g' \
		< "$<" > "$@"
	@chmod +x $@
