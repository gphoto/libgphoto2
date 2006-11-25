#

EXTRA_DIST = gphoto2.i TODO javatest.java.in

data_DATA = javatest.class

# FIXME: Is "sed -i" a standard?
gphoto2_wrap.c: $(srcdir)/gphoto2.i
	mkdir -p swig/org/gphoto2
	$(SWIG) -v -I$(top_builddir) \
		-o gphoto2_wrap.c -oh gphoto2_wrap.h \
		-java -package swig.org.gphoto2 -outdir swig/org/gphoto2 \
		$(srcdir)/gphoto2.i
	sed -i -e 's/ interface)/ interface1)/' \
		swig/org/gphoto2/_GPPortSettingsUSB.java || exit -1

lib_LTLIBRARIES = libgphoto2_jni.la

libgphoto2_jni_la_CPPFLAGS =				\
	$(LTDLINCL)					\
	$(AM_CPPFLAGS) $(CPPFLAGS)			\
	$(LIBEXIF_CFLAGS)

nodist_libgphoto2_jni_la_SOURCES = gphoto2_wrap.c
libgphoto2_jni_la_LIBADD = \
	$(top_builddir)/libgphoto2/libgphoto2.la \
	$(top_builddir)/libgphoto2_port/libgphoto2_port/libgphoto2_port.la \
	$(LIBEXIF_LIBS) \
	$(INTLLIBS)

clean-local:
	rm -rf swig

CLEANFILES = $(nodist_libgphoto2_jni_la_SOURCES) javatest.class javatest.java swig-org-gphoto2.jar

swig-org-gphoto2.jar: gphoto2_wrap.c
	find swig/org/gphoto2 -type f -name '*.java' \
		| xargs javac
	find swig/org/gphoto2 -type f -name '*.class' \
		| xargs jar cf swig-org-gphoto2.jar

javatest.java: $(srcdir)/javatest.java.in swig-org-gphoto2.jar
	cat '$<' > '$@'

javatest.class: javatest.java
	javac -classpath swig-org-gphoto2.jar javatest.java

javatest.sh: Makefile
	echo '###' cp -f "$$PWD/.libs/libgphoto2_jni.so" "..../jre/lib/i386"
	echo java -classpath .:swig-org-gphoto2.jar javatest > '$@'
	chmod +x '$@'

check_SCRIPTS = javatest.sh

TESTS = javatest.sh
