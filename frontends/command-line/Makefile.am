## Process this file with automake to create a GNU Makefile

# We need to add the source files to the list because sometimes 
# people without aalib or cdk make dist.
EXTRA_DIST = makefile.os2				\
	gphoto2-cmd-config.c gphoto2-cmd-config.h	\
	gphoto2-cmd-capture.c gphoto2-cmd-capture.h

bin_PROGRAMS = gphoto2

INCLUDES =						\
	-DGPHOTO2_LOCALEDIR=\""$(datadir)/locale"\"	\
	-DDOC_DIR=\""@DOC_DIR@"\"			\
	-I$(top_srcdir)/libgphoto2_port/libgphoto2_port	\
	-I$(top_srcdir)/libgphoto2			\
	$(CDK_CFLAGS)					\
	$(AA_CFLAGS)					\
	$(EXIF_CFLAGS)					\
	$(RL_CFLAGS)

if HAVE_CDK
CDK_FILES = gphoto2-cmd-config.c gphoto2-cmd-config.h
else
CDK_FILES =
endif

if HAVE_AA
AA_FILES = gphoto2-cmd-capture.c gphoto2-cmd-capture.h
else
AA_FILES =
endif

gphoto2_SOURCES = 		\
	$(AA_FILES)		\
	$(CDK_FILES)		\
	actions.c actions.h 	\
	foreach.c foreach.h 	\
	globals.h 		\
	main.c main.h 		\
	options.c options.h 	\
	range.c range.h 	\
	shell.c shell.h 

gphoto2_LDFLAGS = -export-dynamic

gphoto2_LDADD =						\
	$(CDK_LIBS)					\
	$(top_builddir)/libgphoto2_port/libgphoto2_port/libgphoto2_port.la\
	$(top_builddir)/libgphoto2/libgphoto2.la	\
	$(AA_LIBS)					\
	$(JPEG_LIBS) $(PTHREAD_LIBS)			\
	$(EXIF_LIBS) $(INTLLIBS) $(RL_LIBS)
