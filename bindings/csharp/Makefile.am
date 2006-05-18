# Build libgphoto2-sharp, the C# bindings for libgphoto2.
#
# Due to lacking automake support for mono/C#, we compile the C#
# code as _DATA.
#
# We also have a test case which is called on "make check".
#
# This should be clean for
#   - out-of-tree builds
# 
# It may be buggy for
#   - cross-compiles
#   - C# 2 (gmcs instead of mcs; more issues)
#
# This Makefile.am relies on a few variables defined in configure.in.

MCSFLAGS = -unsafe -debug

pkgconfig_DATA = libgphoto2-sharp.pc

LIBGPHOTO2_SHARP_GENERATED_CSFILES = \
	AssemblyInfo.cs

LIBGPHOTO2_SHARP_CSFILES =				\
	$(srcdir)/Camera.cs				\
	$(srcdir)/CameraAbilitiesList.cs		\
	$(srcdir)/CameraFile.cs				\
	$(srcdir)/CameraFilesystem.cs			\
	$(srcdir)/CameraList.cs				\
	$(srcdir)/CameraWidget.cs			\
	$(srcdir)/Context.cs				\
	$(srcdir)/ErrorCodes.cs				\
	$(srcdir)/Object.cs				\
	$(srcdir)/Port.cs				\
	$(srcdir)/PortInfo.cs				\
	$(srcdir)/PortInfoList.cs

cslib_DATA =				\
	libgphoto2-sharp.dll		\
	libgphoto2-sharp.dll.config	\
	libgphoto2-sharp.dll.mdb

libgphoto2-sharp.dll: $(LIBGPHOTO2_SHARP_GENERATED_CSFILES) $(LIBGPHOTO2_SHARP_CSFILES)
	$(MCS) $(MCSFLAGS) -out:$@ /target:library $(LIBGPHOTO2_SHARP_GENERATED_CSFILES) $(LIBGPHOTO2_SHARP_CSFILES)

TestGphoto2Sharp.exe: $(srcdir)/TestGphoto2Sharp.cs libgphoto2-sharp.dll
	$(MCS) $(MCSFLAGS) -out:$@ -r:libgphoto2-sharp.dll $<

check_SCRIPTS = TestGphoto2Sharp
check_DATA = TestGphoto2Sharp.exe

TESTS = $(check_SCRIPTS)

TestGphoto2Sharp: Makefile $(srcdir)/TestGphoto2Sharp.cs
	echo '#!/bin/sh' > $@
	echo '$(MONO) $$(dirname "$$0")/TestGphoto2Sharp.exe' >> $@
	chmod +x $@

EXTRA_DIST =							\
	libgphoto2-sharp.dll.config				\
	libgphoto2-sharp.pc.in					\
	$(LIBGPHOTO2_SHARP_CSFILES)				\
	AssemblyInfo.cs.in

CLEANFILES =							\
	libgphoto2-sharp.dll.mdb				\
	libgphoto2-sharp.dll					\
	TestGphoto2Sharp

