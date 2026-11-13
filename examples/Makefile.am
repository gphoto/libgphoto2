EXTRA_DIST = README sample-libfuzz.c
EXTRA_PROGRAMS =
check_PROGRAMS =
check_SCRIPTS =
noinst_PROGRAMS =


AM_CPPFLAGS += -I$(top_srcdir) -I$(top_builddir) -I$(top_srcdir)/libgphoto2_port

CLEANFILES = $(check_SCRIPTS)


noinst_PROGRAMS += \
	sample-autodetect sample-multi-detect \
	lunkwill-canon-capture  \
	sample-owner \
	sample-photobooth \
	sample-preview \
	sample-tether \
	sample-afl \
	best-iso \
	sample-capture sample-trigger-capture sample-justfocus

sample_afl_SOURCES = sample-afl.c samples.h context.c autodetect.c
sample_autodetect_SOURCES = sample-autodetect.c samples.h context.c config.c
sample_owner_SOURCES = sample-owner.c samples.h context.c config.c
sample_multi_detect_SOURCES = sample-multi-detect.c samples.h autodetect.c context.c config.c
lunkwill_canon_capture_SOURCES = lunkwill-canon-capture.c context.c samples.h config.c
sample_capture_SOURCES = sample-capture.c context.c samples.h config.c focus.c
sample_trigger_capture_SOURCES = sample-trigger-capture.c context.c samples.h config.c
sample_photobooth_SOURCES = sample-photobooth.c samples.h context.c config.c
sample_preview_SOURCES = preview.c samples.h context.c config.c focus.c
sample_tether_SOURCES = samples.h sample-tether.c context.c config.c
best_iso_SOURCES = best-iso.c
sample_justfocus_SOURCES = samples.h sample-justfocus.c context.c focus.c
# sample_libfuzz_SOURCES = samples.h sample-libfuzz.c context.c autodetect.c

LDADD = \
	$(libgphoto2_la) \
	$(libgphoto2_port_la) \
	$(LIBLTDL) \
	$(LIBEXIF_LIBS) \
	$(INTLLIBS)
