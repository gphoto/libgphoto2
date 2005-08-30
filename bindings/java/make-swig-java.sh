MYCAMERADRIVER=ptp2
MYCAMLIBDIR=/usr/lib/gphoto2/2.1.5
MYIOLIBDIR=/usr/lib/gphoto2_port/0.5.1
MYLOCALEDIR=/usr/share/locale
LIBGPHOTO2SRCDIR=..
LIBGPHOTO2PORTSRCDIR=../libgphoto2_port
JDKHOME=/opt/SUNWappserver/jdk
MYINCL="-I${JDKHOME}/include -I${JDKHOME}/include/linux -I${LIBGPHOTO2SRCDIR} -I${LIBGPHOTO2SRCDIR}/libgphoto2 -I${LIBGPHOTO2PORTSRCDIR} -I${LIBGPHOTO2PORTSRCDIR}/libgphoto2_port -I${LIBGPHOTO2SRCDIR}/intl"
MYLDADD=""
EXTRACFLAGS=" -DIOLIBS=\"${MYIOLIBDIR}\" -DCAMLIBS=\"${MYCAMLIBDIR}\" -DLOCALEDIR=\"${MYLOCALEDIR}\" "
GPSOURCEFILES="${LIBGPHOTO2SRCDIR}/libgphoto2/gphoto2-abilities-list.c ${LIBGPHOTO2SRCDIR}/libgphoto2/gphoto2-camera.c ${LIBGPHOTO2SRCDIR}/libgphoto2/gphoto2-context.c ${LIBGPHOTO2SRCDIR}/libgphoto2/gphoto2-file.c ${LIBGPHOTO2SRCDIR}/libgphoto2/gphoto2-filesys.c ${LIBGPHOTO2SRCDIR}/libgphoto2/gphoto2-list.c ${LIBGPHOTO2SRCDIR}/libgphoto2/gphoto2-result.c ${LIBGPHOTO2SRCDIR}/libgphoto2/gphoto2-setting.c ${LIBGPHOTO2SRCDIR}/libgphoto2/gphoto2-version.c ${LIBGPHOTO2SRCDIR}/libgphoto2/gphoto2-widget.c"
GPPORTSOURCEFILES="${LIBGPHOTO2PORTSRCDIR}/libgphoto2_port/gphoto2-port.c ${LIBGPHOTO2PORTSRCDIR}/libgphoto2_port/gphoto2-port-info-list.c ${LIBGPHOTO2PORTSRCDIR}/libgphoto2_port/gphoto2-port-log.c ${LIBGPHOTO2PORTSRCDIR}/libgphoto2_port/gphoto2-port-portability.c ${LIBGPHOTO2PORTSRCDIR}/libgphoto2_port/gphoto2-port-result.c ${LIBGPHOTO2PORTSRCDIR}/libgphoto2_port/gphoto2-port-version.c"


echo "Generating SWIG files..."
echo " *"
echo " * Messages such as \"Warning(305): Bad constant value (ignored)\" are expected and ignoreable"
echo " *"
mkdir -p swig/org/gphoto2
swig -I${LIBGPHOTO2SRCDIR} -I${LIBGPHOTO2PORTSRCDIR} -java -package swig.org.gphoto2 -outdir swig/org/gphoto2 gphoto2.i || exit -1
echo "Compiling..."
gcc -fpic ${MYINCL} ${EXTRACFLAGS} -c ${GPSOURCEFILES} ${GPPORTSOURCEFILES} gphoto2_wrap.c || exit -1
echo "Linking..."
gcc -shared -fpic ${MYLDADD} ${EXTRACFLAGS} -lgphoto2 -lexif -L${MYCAMLIBDIR} -lgphoto2_${MYCAMERADRIVER} ${MYINCL} ${GPSOURCEFILES} ${GPPORTSOURCEFILES} gphoto2_wrap.o -o libgphoto2_jni.so || exit -1

# patch a couple of things in the swig-generated files
#sed -i -e 's/private long swigCPtr;/public long swigCPtr;/' swig/org/gphoto2/_GPPortInfo.java
#sed -i -e 's/private long swigCPtr;/public long swigCPtr;/' swig/org/gphoto2/port/_GPPortInfo.java
sed -i -e 's/ interface)/ interface1)/' ./swig/org/gphoto2/_GPPortSettingsUSB.java || exit -1

echo "Compiling Java files..."
find ./swig/org/gphoto2 -name "*.java" | xargs javac  || exit -1

echo "Creating JAR archive..."
find ./swig/org/gphoto2 -name "*.class" | xargs jar cf swig-org-gphoto2.jar  || exit -1

# compile test program
cp javatest.java.in javatest.java || exit -1
javac -classpath swig-org-gphoto2.jar javatest.java || exit -1

echo cp -f libgphoto2_jni.so ${JDKHOME}/jre/lib/i386/
echo java -classpath .:swig-org-gphoto2.jar javatest || exit -1
