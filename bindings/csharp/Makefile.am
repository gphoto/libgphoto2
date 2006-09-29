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

if HAVE_MONO

MCSFLAGS = -unsafe -debug

pkgconfig_DATA = libgphoto2-sharp.pc

LIBGPHOTO2_SHARP_GENERATED_CSFILES = \
	AssemblyInfo.cs

cslib_DATA =				\
	libgphoto2-sharp.dll		\
	libgphoto2-sharp.dll.config	\
	libgphoto2-sharp.dll.mdb

libgphoto2-sharp.dll: $(LIBGPHOTO2_SHARP_GENERATED_CSFILES) $(LIBGPHOTO2_SHARP_CSFILES)
	$(MCS) $(MCSFLAGS) -out:$@ /target:library $(LIBGPHOTO2_SHARP_GENERATED_CSFILES) $(LIBGPHOTO2_SHARP_CSFILES)

TestGphoto2Sharp.exe: $(srcdir)/TestGphoto2Sharp.cs libgphoto2-sharp.dll
	$(MCS) $(MCSFLAGS) -out:$@ -r:libgphoto2-sharp.dll $<

TestSizes.exe: TestSizes.cs libgphoto2-sharp.dll
	$(MCS) $(MCSFLAGS) -out:$@ -r:libgphoto2-sharp.dll $<

check_SCRIPTS = check-camera-list.sh check-sizes.sh
check_DATA = TestGphoto2Sharp.exe TestSizes.exe
check_PROGRAMS = testsizes

BUILT_SOURCES = testsizes.c TestSizes.cs
testsizes_SOURCES = testsizes.c

TESTS = check-sizes.sh
INSTALL_TESTS = check-camera-list.sh

CLEANFILES =							\
	TestGphoto2Sharp.exe					\
	TestGphoto2Sharp.exe.mdb				\
	TestSizes.exe						\
	TestSizes.exe.mdb					\
	libgphoto2-sharp.dll.mdb				\
	libgphoto2-sharp.dll					\
	$(check_SCRIPTS)					\
	$(BUILT_SOURCES)

endif

EXTRA_DIST =							\
	libgphoto2-sharp.dll.config				\
	libgphoto2-sharp.pc.in					\
	$(LIBGPHOTO2_SHARP_CSFILES)				\
	AssemblyInfo.cs.in					\
	TestGphoto2Sharp.cs					\
	testsizes-typelist.txt					\
	testsizes-createsource.sh				\
	check-sizes.sh.in					\
	check-camera-list.sh.in

testsizes.c: testsizes-typelist.txt $(srcdir)/testsizes-createsource.sh
	sh $(srcdir)/testsizes-createsource.sh c < $< > $@

TestSizes.cs: testsizes-typelist.txt $(srcdir)/testsizes-createsource.sh
	sh $(srcdir)/testsizes-createsource.sh csharp < $< > $@

include $(top_srcdir)/installcheck.mk
