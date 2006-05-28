#

EXTRA_DIST = gphoto2.i TODO javatest.java.in

data_DATA = gphoto2.i

gphoto2_wrap.c: $(srcdir)/gphoto2.i
	mkdir -p swig/org/gphoto2
	$(SWIG) -v -I$(top_srcdir) \
		-o gphoto2_wrap.c -oh gphoto2_wrap.h \
		-java -package swig.org.gphoto2 -outdir swig/org/gphoto2 \
		$(srcdir)/gphoto2.i

lib_LTLIBRARIES = libgphoto2_jni.la

libgphoto2_jni_la_CPPFLAGS =				\
	$(LTDLINCL)					\
	$(AM_CPPFLAGS) $(CPPFLAGS)			\
	$(LIBEXIF_CFLAGS)

libgphoto2_jni_la_SOURCES = gphoto2_wrap.c
libgphoto2_jni_la_LIBADD = \
	$(top_builddir)/libgphoto2/libgphoto2.la \
	$(top_builddir)/libgphoto2_port/libgphoto2_port/libgphoto2_port.la \
	$(LIBEXIF_LIBS) \
	$(INTLLIBS)
