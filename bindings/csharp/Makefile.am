CSC = mcs --unsafe -g

LIBGPHOTO2_SHARP_CSFILES =				\
	$(srcdir)/AssemblyInfo.cs			\
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

libgphoto2sharplibdir = $(libdir)/f-spot
libgphoto2sharplib_DATA =		\
	libgphoto2-sharp.dll		\
	libgphoto2-sharp.dll.config


libgphoto2-sharp.dll: $(LIBGPHOTO2_SHARP_CSFILES)
	$(CSC) -o $@ /target:library $(LIBGPHOTO2_SHARP_CSFILES)

all: libgphoto2-sharp.dll

EXTRA_DIST =							\
	libgphoto2-sharp.dll.config				\
	$(LIBGPHOTO2_SHARP_CSFILES)

CLEANFILES =							\
	libgphoto2-sharp.dll.mdb				\
	libgphoto2-sharp.dll

