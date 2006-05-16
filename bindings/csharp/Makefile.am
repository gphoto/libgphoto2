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

EXTRA_DIST =							\
	libgphoto2-sharp.dll.config				\
	libgphoto2-sharp.pc.in					\
	$(LIBGPHOTO2_SHARP_CSFILES)				\
	AssemblyInfo.cs.in

CLEANFILES =							\
	libgphoto2-sharp.dll.mdb				\
	libgphoto2-sharp.dll

