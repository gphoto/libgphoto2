## Process this file with automake to create a GNU Makefile

EXTRA_DIST = makefile.os2

bin_PROGRAMS = gphoto2

CFLAGS = @CFLAGS@ -g 

INCLUDES =						\
	-DGPHOTO2_LOCALEDIR=\""$(datadir)/locale"\"	\
	-I$(top_srcdir)/libgphoto2_port/libgphoto2_port	\
	-I$(top_srcdir)/libgphoto2			\
	$(CDK_CFLAGS)					\
	$(AA_CFLAGS)

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

LDFLAGS = @LDFLAGS@ -g 
gphoto2_SOURCES = 		\
	$(AA_FILES)		\
	$(CDK_FILES)		\
	actions.c actions.h 	\
	foreach.c foreach.h 	\
	globals.h 		\
	interface.c interface.h \
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
	$(JPEG_LIBS) 					\
	$(INTLLIBS)
