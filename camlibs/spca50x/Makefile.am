camlib_LTLIBRARIES = libgphoto2_spca50x.la

EXTRA_DIST = ChangeLog.benq ChangeLog.gsmart ChangeLog.spca504_flash lsusb-v

libgphoto2_spca50x_la_SOURCES = spca50x.c spca50x.h \
				spca50x-flash.c spca50x-flash.h \
				spca50x-sdram.c spca50x-sdram.h \
				library.c \
                               spca50x-jpeg-header.h spca50x-registers.h \
			       spca50x-avi-header.h
libgphoto2_spca50x_la_LDFLAGS = -module -avoid-version
libgphoto2_spca50x_la_LIBADD = $(top_builddir)/libgphoto2/libgphoto2.la
