dnl Process this file with autoreconf to produce a configure script.
AC_PREREQ(2.59)

dnl NOTE: Be version style _higher_ than the last release.
dnl       So lastversion.X.trunk for instance. Bump X if necessary.

dnl ******* THE NEXT RELEASE VERSION MUST BE 2.2.2 at least or higher!
AC_INIT([libgphoto2], [2.2.1.5.trunk], [gphoto-devel@lists.sourceforge.net], [libgphoto2])
AC_CONFIG_SRCDIR([libgphoto2/gphoto2/gphoto2.h])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4m])
AC_CONFIG_AUX_DIR([.])
AM_INIT_AUTOMAKE([-Wall gnu 1.9 dist-bzip2])

GP_CHECK_SHELL_ENVIRONMENT
GP_CONFIG_MSG([Build])
GP_CONFIG_MSG([Source code location],[${srcdir}])


dnl ---------------------------------------------------------------------------
dnl Advanced information about versioning:
dnl   * "Writing shared libraries" by Mike Hearn
dnl         http://navi.cx/~mike/writing-shared-libraries.html
dnl   * libtool.info chapter "Versioning"
dnl   * libtool.info chapter "Updating library version information"
dnl ---------------------------------------------------------------------------
dnl Versioning:
dnl  - CURRENT (Major):  Increment if the interface has changes. AGE is always
dnl                      *changed* at the same time.
dnl  - AGE (Micro):      Increment if any interfaces have been added; set to 0
dnl		         if any interfaces have been removed. Removal has 
dnl                      precedence over adding, so set to 0 if both happened.
dnl                      It denotes upward compatibility.
dnl  - REVISION (Minor): Increment any time the source changes; set to 
dnl			 0 if you incremented CURRENT.
dnl
dnl  To summarize. Any interface *change* increment CURRENT. If that interface
dnl  change does not break upward compatibility (ie it is an addition), 
dnl  increment AGE, Otherwise AGE is reset to 0. If CURRENT has changed, 
dnl  REVISION is set to 0, otherwise REVISION is incremented.
dnl ---------------------------------------------------------------------------
dnl A:R:C=1:1:3  libgphoto2-2.2
dnl A:R:C=2:0:4  libgphoto2-2.3 (since libgphoto2-2.2.1.5.trunk)
AC_SUBST([LIBGPHOTO2_AGE],      [2])
AC_SUBST([LIBGPHOTO2_REVISION], [0])
AC_SUBST([LIBGPHOTO2_CURRENT],  [4])
AC_SUBST([LIBGPHOTO2_CURRENT_MIN],
         [`expr $LIBGPHOTO2_CURRENT - $LIBGPHOTO2_AGE`])
AC_SUBST([LIBGPHOTO2_VERSION_INFO],
         [$LIBGPHOTO2_CURRENT:$LIBGPHOTO2_REVISION:$LIBGPHOTO2_AGE])


dnl Set up compiler options
dnl "CPPFLAGS" concerns the C preprocessor, i.e. both C and C++ source code.
dnl C only would be CFLAGS, C++ only CXXFLAGS
AM_CPPFLAGS="$CPPFLAGS"

AM_CPPFLAGS="$AM_CPPFLAGS -I\$(top_srcdir)"
AM_CPPFLAGS="$AM_CPPFLAGS -I\$(top_srcdir)/libgphoto2_port/libgphoto2_port"
AM_CPPFLAGS="$AM_CPPFLAGS -I\$(top_srcdir)/libgphoto2_port"
AM_CPPFLAGS="$AM_CPPFLAGS -I\$(top_srcdir)/libgphoto2"
AM_CPPFLAGS="$AM_CPPFLAGS -I\$(top_builddir)/libgphoto2"


dnl ---------------------------------------------------------------------------
dnl Checks for programs.
dnl ---------------------------------------------------------------------------

AC_PROG_CC
AC_PROG_CPP
AC_PROG_CXX
AC_PROG_INSTALL

dnl Something with the sequences is not quite alright yet.
dnl SED is supposed to be set in AC_LIBTOOL_SETUP, but the
dnl sequence seems to get mixed up.
SED=${SED-sed}


try_gp2ddb=false
msg_gp2ddb=""
gp_msg_dp2ddb="no"

AC_ARG_ENABLE([gp2ddb],
[AS_HELP_STRING([--enable-gp2ddb],
		[enable EXPERIMENTAL PRE-ALPHA gphoto2 device database code])],[
	if test "x$enableval" = "xyes"; then
		try_gp2ddb=true
		msg_dp2ddb="(--enable-gp2ddb detected)"
	elif test "x$enableval" = "xno"; then
		try_gp2ddb=false
		msg_dp2ddb="(--disable-gp2ddb detected)"
	fi
])

AC_PATH_PROG([FLEX], [flex])
AC_SUBST([FLEX])
AC_PATH_PROG([BISON], [bison])
AC_SUBST([BISON])

AC_MSG_CHECKING([whether to enable GP2DDB])
if "$try_gp2ddb"; then
   if test -x "$FLEX" && test -x "$BISON"; then
      gp_msg_dp2ddb="yes $msg_dp2ddb"
   else
      AC_MSG_ERROR([Fatal: GP2DDB support requires flex and bison!

Aborting configuration.

Install flex and bison into your PATH or run configure with --disable-gp2ddb
to disable GP2DDB support.
])
   fi
else
      gp_msg_dp2ddb="no $msg_dp2ddb"
fi
AC_MSG_RESULT([${gp_msg_gp2ddb}])

AM_CONDITIONAL([HAVE_FLEX_BISON], ["$try_gp2ddb" && test -x "$FLEX" && test -x "$BISON"])

if "$try_gp2ddb" && test -x "$FLEX" && test -x "$BISON"; then
	gp_msg_gp2ddb="yes $msg_gp2ddb"
else
	gp_msg_gp2ddb="no $msg_gp2ddb"
fi
GP_CONFIG_MSG([GP2DDB support], [${gp_msg_gp2ddb} (EXPERIMENTAL)])

GP_PKG_CONFIG


dnl --------------------------------------------------------------------
dnl Check requirements for bindings
dnl --------------------------------------------------------------------

dnl Check for SWIG

AC_PATH_PROG([SWIG], [swig])
AC_SUBST([SWIG])
AM_CONDITIONAL([HAVE_SWIG], [test -x "$SWIG"])

AC_PATH_PROG([JAVAC], [javac])
AC_SUBST([JAVAC])
AC_PATH_PROG([JAVA], [java])
AC_SUBST([JAVA])
AM_CONDITIONAL([HAVA_JDK], [test -x "$JAVAC" && test -x "$JAVA"])

if false && test -x "$SWIG" && test -x "$JAVAC" && test -x "$JAVA"; then
   bindings="${bindings}${bindings+ }java"
fi

dnl Check for Mono and Mono C# compiler from f-spot

dnl --- Check for mono and mcs
dnl --- FIXME: What about when mono and mcs are not found?

AC_PATH_PROG([MONO], [mono])
AC_PATH_PROG([MCS], [mcs])
AC_SUBST([MONO])
AC_SUBST([MCS])

dnl -- Check for mono pc file

AC_MSG_CHECKING([for mono.pc])
if test -z `$PKG_CONFIG --variable=prefix mono`; then
  build_binding_csharp="no"
  AC_MSG_RESULT([missing the mono.pc file, usually found in the mono-devel package])
else
  build_binding_csharp="yes"
  bindings="${bindings}${bindings+ }csharp"
  AC_MSG_RESULT([found])
fi
AC_SUBST([cslibdir],[${libdir}/${PACKAGE_TARNAME}])

AM_CONDITIONAL([HAVE_MONO], [test "x${build_binding_csharp}" = "xyes"])
GP_CONFIG_MSG([Bindings], [${bindings}])

AC_SUBST([BINDING_SUBDIRS], [${bindings}])

dnl check for/set up libtool and libltdl
dnl AC_DISABLE_STATIC
dnl AC_DISABLE_SHARED
AC_LIBLTDL_CONVENIENCE([libltdl])
AC_WITH_LTDL
AC_LIBTOOL_WIN32_DLL
AC_LIBTOOL_DLOPEN
AC_PROG_LIBTOOL
AC_SUBST([LTDLINCL])
AC_SUBST([LIBLTDL])

# report on compiler/libtool setup
GP_CONFIG_MSG([Compiler],[${CC}])
GP_CONFIG_MSG([libltdl includes],[$LTDLINCL])
GP_CONFIG_MSG([libltdl library],[$LIBLTDL])

AC_DEFINE_UNQUOTED(HAVE_CC,"$CC",[The C compiler we're using])


dnl ---------------------------------------------------------------------------
dnl Reference definitions
dnl    required before we do the Makevars hack!
dnl ---------------------------------------------------------------------------

GP_REFERENCES()dnl


GP_CONFIG_MSG([Features])

dnl ---------------------------------------------------------------------------
dnl i18n support (including some hacks)
dnl ---------------------------------------------------------------------------

ALL_LINGUAS="da de es eu fr hu it ja nl nb pl ru sl sv uk zh_CN"
GP_GETTEXT_HACK([${PACKAGE}-${LIBGPHOTO2_CURRENT_MIN}],[The gPhoto Team],[${MAIL_GPHOTO_TRANSLATION}])
AM_GNU_GETTEXT_VERSION([0.14.1])
AM_GNU_GETTEXT([external])
AM_PO_SUBDIRS()
AM_ICONV()
GP_GETTEXT_FLAGS()

dnl FIXME: We have to make sure this works first
dnl AC_CHECK_FUNC(gettext, gettext_without_libintl=true)
# same trick as with libdl:
# if gettext() doesn't require linking against libintl,
# we don't have to check for gettext in libintl. Otherwise
# we may even require libintl.
dnl if test "$gettext_without_libintl" != "true" -a "$USE_NLS" = "yes"; then
dnl        AC_CHECK_LIB([intl], [gettext], [INTLLIBS="$INTLLIBS -lintl"])
dnl fi

AC_SUBST([localedir],["\$(datadir)/locale"])
AM_CPPFLAGS="$AM_CPPFLAGS -DLOCALEDIR=\\\"${localedir}\\\""


dnl ---------------------------------------------------------------------------
dnl FIXME: What is this good for?
dnl Replace `main' with a function in -libs:
dnl ---------------------------------------------------------------------------
AC_CHECK_LIB(ibs, main)


dnl ---------------------------------------------------------------------------
dnl check for libjpeg
dnl ---------------------------------------------------------------------------
LIBJPEG=""
libjpeg_msg="no"
AC_SUBST(LIBJPEG)
AC_CHECK_LIB(jpeg,jpeg_start_decompress,[
	AC_CHECK_HEADER(jpeglib.h,[
		AC_DEFINE(HAVE_LIBJPEG,1,[define if we found LIBJPEG and its headers])
		LIBJPEG="-ljpeg"
		libjpeg_msg="yes"
	])
])
GP_CONFIG_MSG([JPEG mangling support],[${libjpeg_msg}])


dnl ---------------------------------------------------------------------------
dnl Checks for header files.
dnl ---------------------------------------------------------------------------
AC_HEADER_DIRENT
# before _HEADER_STDC
AC_HEADER_STDC
# after _HEADER_STDC
AC_CHECK_HEADERS(getopt.h unistd.h mcheck.h)
AC_C_INLINE([])
AC_C_CONST([])
dnl FIXME: AC_STRUCT_TIMEZONE


dnl ---------------------------------------------------------------------------
dnl more header/function/def checks
dnl ---------------------------------------------------------------------------

dnl Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

dnl Checks for library functions.
AC_CHECK_FUNCS([getopt getopt_long mkdir strdup strncpy strcpy snprintf sprintf vsnprintf gmtime_r])

dnl Find out how to get struct tm
AC_STRUCT_TM

dnl Check for tm_gmtoff in struct tm
AC_MSG_CHECKING([for tm_gmtoff in struct tm])
AC_COMPILE_IFELSE([dnl
AC_LANG_PROGRAM([[
#include <sys/types.h>
#include <$ac_cv_struct_tm>
]], [[
struct tm tm;
tm.tm_gmtoff;
]])dnl
],[
AC_DEFINE(HAVE_TM_GMTOFF,1,whether struct tm has tm_gmtoff field)
AC_MSG_RESULT(yes)
],[
AC_MSG_RESULT(no)
])


dnl Create a stdint.h-like file containing size-specific integer definitions
dnl that will always be available
AX_NEED_STDINT_H([libgphoto2/_stdint.h])

dnl Create a header file containing NetBSD-style byte swapping macros
AC_NEED_BYTEORDER_H(libgphoto2/gphoto2/gphoto2-endian.h)


dnl ---------------------------------------------------------------------------
dnl Old libtool stuff left here for reference purposes. For now.
dnl dnl ---------------------------------------------------------------------------
dnl dnl Check for libtool: lt_dlforeachfile has been introduced in 
dnl dnl		       libtool-1.4. However, there are still systems out
dnl dnl		       there running libtool-1.3. For those, we will need
dnl dnl		       dlopen. Note that on some systems (e.g. FreeBSD)
dnl dnl		       -ldl isn't needed.
dnl dnl ---------------------------------------------------------------------------
dnl AC_PROG_LIBTOOL
dnl have_ltdl=false
dnl ltdl_msg="no"
dnl try_ltdl=false
dnl AC_ARG_WITH(ltdl, [  --with-ltdl             Use ltdl],[
dnl         if test x$withval = xyes; then
dnl 		try_ltdl=true
dnl 	fi])
dnl if $try_ltdl; then
dnl 	AC_CHECK_LIB(ltdl, lt_dlforeachfile,[
dnl 		AC_CHECK_HEADER(ltdl.h,[
dnl 			AC_DEFINE(HAVE_LTDL,1,[whether we use libltdl])
dnl 			LTDL_LIBS="-lltdl"
dnl 			ltdl_msg="yes"
dnl 			have_ltdl=true])])
dnl fi
dnl if ! $have_ltdl; then
dnl 	AC_CHECK_FUNC(dlopen,,[
dnl 		AC_CHECK_LIB(dl, dlopen,[LTDL_LIBS="-ldl"],[AC_ERROR([
dnl *** Could not determine how to handle
dnl *** shared libraries!])])])
dnl fi
dnl AC_SUBST(LTDL_LIBS)


dnl should be obsolete (we're not using grep and tr any more)
dnl dnl Solaris hack for grep and tr
dnl [
dnl if test -n "`echo $host_os | grep '[sS]olaris'`"; then
dnl   TR=/usr/xpg4/bin/tr
dnl   GREP=/usr/xpg4/bin/grep  
dnl else
dnl   TR=tr
dnl   GREP=grep
dnl fi
dnl ]


dnl ---------------------------------------------------------------------------
dnl utility installation directory
dnl
dnl If your system wants utilities installed into ${libexecdir} instead, then
dnl run configure with 'utilsdir=${libexecdir}/${PACKAGE_TARNAME}'.
dnl ---------------------------------------------------------------------------

AC_SUBST([PACKAGE_TARNAME])
utilsdir="\${libdir}/\${PACKAGE_TARNAME}"
AC_ARG_VAR([utilsdir],[Directory where utilities like print-camera-list will be installed])
AC_SUBST([utilsdir])


dnl ---------------------------------------------------------------------------
dnl camlib installation directory
dnl ---------------------------------------------------------------------------
AC_MSG_CHECKING([where to install camlibs])
AC_ARG_WITH([camlibdir],[AS_HELP_STRING(
	[--with-camlibdir=<path>],
	[install camlibs in directory <path>])dnl
],[
	camlibdir="$withval"
],[
	camlibdir="\${libdir}/${PACKAGE_TARNAME}/${PACKAGE_VERSION}"
])
AC_MSG_RESULT([${camlibdir}])
AC_SUBST([camlibdir])
AM_CPPFLAGS="$AM_CPPFLAGS -DCAMLIBS=\\\"\$(camlibdir)\\\""

CAMLIB_LDFLAGS="-module -no-undefined -avoid-version -export-dynamic"
CAMLIB_LDFLAGS="$CAMLIB_LDFLAGS -export-symbols \$(top_srcdir)/camlibs/camlib.sym"
AC_SUBST([CAMLIB_LDFLAGS],[${CAMLIB_LDFLAGS}])
AC_SUBST([CAMLIB_LIBS],["\$(top_builddir)/libgphoto2/libgphoto2.la"])


dnl ---------------------------------------------------------------------------
dnl Camera drivers (camlibs)
dnl ---------------------------------------------------------------------------
dnl * See m4m/gp-camlibs.m4 for usage details, e.g. for marking camlibs as
dnl   obsolete.
dnl * Please add new camlibs in alphabetical order.
GP_CAMLIB([adc65])dnl
GP_CAMLIB([agfa-cl20])dnl
GP_CAMLIB([aox])dnl
GP_CAMLIB([barbie])dnl
GP_CAMLIB([canon])dnl
GP_CAMLIB([casio])dnl
GP_CAMLIB([clicksmart310])dnl
GP_CAMLIB([digigr8])dnl
GP_CAMLIB([digita])dnl
GP_CAMLIB([dimera])dnl
GP_CAMLIB([directory])dnl
GP_CAMLIB([enigma13])dnl
GP_CAMLIB([fuji])dnl
GP_CAMLIB([gsmart300])dnl
GP_CAMLIB([hp215])dnl
GP_CAMLIB([iclick])dnl
GP_CAMLIB([jamcam])dnl
GP_CAMLIB([jd11])dnl
GP_CAMLIB([kodak])dnl
GP_CAMLIB([konica])dnl
GP_CAMLIB([largan])dnl
GP_CAMLIB([lg_gsm])dnl
GP_CAMLIB([mars])dnl
GP_CAMLIB([minolta])dnl
GP_CAMLIB([mustek])dnl
GP_CAMLIB([panasonic])dnl
GP_CAMLIB([pccam300])dnl
GP_CAMLIB([pccam600])dnl
GP_CAMLIB([polaroid])dnl
GP_CAMLIB([ptp2])dnl
GP_CAMLIB([ricoh])dnl
GP_CAMLIB([samsung])dnl
GP_CAMLIB([sierra])dnl
GP_CAMLIB([sipix])dnl
GP_CAMLIB([smal])dnl
GP_CAMLIB([sonix])dnl
GP_CAMLIB([sonydscf1])dnl
GP_CAMLIB([sonydscf55])dnl
GP_CAMLIB([soundvision])dnl
GP_CAMLIB([spca50x])dnl
GP_CAMLIB([sq905])dnl
GP_CAMLIB([stv0674])dnl
GP_CAMLIB([stv0680])dnl
GP_CAMLIB([sx330z])dnl
GP_CAMLIB([template])dnl
GP_CAMLIB([toshiba])dnl
dnl
GP_CAMLIBS_DEFINE()dnl


dnl --------------------------------------------------------------------
dnl documentation
dnl --------------------------------------------------------------------
GP_CHECK_DOC_DIR()dnl
GP_CHECK_DOXYGEN()dnl
GP_BUILD_GTK_DOCS()dnl
AC_SUBST([camlibdocdir], ['${docdir}/camlibs'])


dnl --------------------------------------------------------------------
dnl initialize convenience packaging
dnl --------------------------------------------------------------------
GPKG_CHECK_RPM()dnl Check wether to package for a system using RPM
GPKG_CHECK_LINUX()dnl Check wether to package for a linux system


dnl --------------------------------------------------------------------
dnl guess directory to install *.pc into
dnl --------------------------------------------------------------------
pkgconfigdir='${libdir}/pkgconfig'
AC_SUBST([pkgconfigdir])


dnl ---------------------------------------------------------------------------
dnl libexif: The CameraFilesystem can use libexif for extracting thumbnails
dnl	     out of EXIF data. Similarly, it can extract the mtime of
dnl	     a file. 
dnl	     libexif is available from
dnl	     http://www.sourceforge.net/projects/libexif
dnl ---------------------------------------------------------------------------
GP_CHECK_LIBRARY([LIBEXIF],[libexif],[>= 0.6.13],
		 [libexif/exif-data.h],[exif_data_new],[
AC_MSG_CHECKING([whether we use a version of libexif with ExifData.ifd[[]]])
# Check for libexif version
dnl FIXME: Use AC_CHECK_MEMBER?
CPPFLAGS_save="$CPPFLAGS"
CPPFLAGS="$LIBEXIF_CFLAGS"
AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
/* CC=$CC */
/* CPPFLAGS=${CPPFLAGS} */
/* LDFLAGS=${LDFLAGS} */
#include <libexif/exif-data.h>
#include <libexif/exif-content.h>
]],[[
	ExifData ed;
	ExifContent *ec0, *ec1;
	ec0 = &ed.ifd[0];
	ec1 = &ed.ifd[1];
]])], [
   AC_DEFINE([HAVE_LIBEXIF_IFD],1,
	     [whether we use a version of libexif with ExifData.ifd[[]]])
   AC_MSG_RESULT([yes])
], [
   AC_MSG_RESULT([no])
])
CPPFLAGS="$CPPFLAGS_save"
],[],[default-on],[http://www.sourceforge.net/projects/libexif])dnl


dnl ---------------------------------------------------------------------------
dnl Check libusb
dnl FIXME: What for? Shouldn't this belong into libgphoto2_port?
dnl ---------------------------------------------------------------------------
AC_PATH_PROG(LIBUSB_CONFIG,libusb-config)
AM_CONDITIONAL(HAVE_LIBUSB,test -n "${LIBUSB_CONFIG}")


dnl ---------------------------------------------------------------------------
dnl Turn on all warnings when using gcc
dnl ---------------------------------------------------------------------------
if test "x$GCC" = "xyes"; then
	AM_CPPFLAGS="$AM_CPPFLAGS -Wall"
	AM_CFLAGS="$AM_CFLAGS -Wmissing-declarations -Wmissing-prototypes"
	AM_LDFLAGS="$AM_LDFLAGS -Wall"
	# FIXME: Check for --enable-debug here
	AM_CPPFLAGS="$AM_CPPFLAGS -g"
	AM_LDFLAGS="$AM_LDFLAGS -g"
fi
AM_CONDITIONAL([HAVE_GCC], [test "x$GCC" = "xyes"])

AM_CPPFLAGS="$AM_CPPFLAGS -D_GPHOTO2_INTERNAL_CODE"

dnl FIXME: Is this accurate?
AM_CONDITIONAL([HAVE_CXX], [test -n "$CXX" && test "X$CXX" != "Xno"])

# CPPFLAGS: C PreProcessor flags
AC_SUBST([AM_CPPFLAGS])
# CFLAGS: C compiler flags
AC_SUBST([AM_CFLAGS])
# CXXFLAGS: C++ compiler flags
AC_SUBST([AM_CXXFLAGS])
# LDFLAGS: Linker flags
AC_SUBST([AM_LDFLAGS])


dnl ---------------------------------------------------------------------------
dnl Configure subprojects
dnl ---------------------------------------------------------------------------
if test -d "$srcdir/libgphoto2_port"; then
	AC_CONFIG_SUBDIRS([libgphoto2_port])
	GP_AUTOGEN_SUBDIR([libgphoto2_port])
else
	AC_MSG_ERROR([
*** Hey, where have you hidden my libgphoto2_port?
*** I needed that!
])
fi


# ---------------------------------------------------------------------------
# Remove obsolete directory of copied files/symlinks
# ---------------------------------------------------------------------------

if test -d gphoto2; then rm -f gphoto2/gphoto2*.h; rmdir gphoto2; fi


# ---------------------------------------------------------------------------
# Create output files
# ---------------------------------------------------------------------------
AC_CONFIG_FILES([
README
Makefile
gphoto2-config
camlibs/Makefile
camlibs/adc65/Makefile
camlibs/agfa-cl20/Makefile
camlibs/aox/Makefile
camlibs/barbie/Makefile
camlibs/canon/Makefile
camlibs/canon/doc/Makefile
camlibs/casio/Makefile
camlibs/clicksmart310/Makefile
camlibs/digigr8/Makefile
camlibs/digita/Makefile
camlibs/dimera/Makefile
camlibs/directory/Makefile
camlibs/fuji/Makefile
camlibs/gsmart300/Makefile
camlibs/hp215/Makefile
camlibs/iclick/Makefile
camlibs/jamcam/Makefile
camlibs/jd11/Makefile
camlibs/kodak/Makefile
camlibs/kodak/dc120/Makefile
camlibs/kodak/dc210/Makefile
camlibs/kodak/dc240/Makefile
camlibs/kodak/dc3200/Makefile
camlibs/kodak/ez200/Makefile
camlibs/konica/Makefile
camlibs/konica/localization/Makefile
camlibs/largan/Makefile
camlibs/largan/lmini/Makefile
camlibs/lg_gsm/Makefile
camlibs/mars/Makefile
camlibs/minolta/Makefile
camlibs/minolta/dimagev/Makefile
camlibs/mustek/Makefile
camlibs/panasonic/Makefile
camlibs/panasonic/l859/Makefile
camlibs/panasonic/coolshot/Makefile
camlibs/pccam300/Makefile
camlibs/pccam600/Makefile
camlibs/polaroid/Makefile
camlibs/ptp2/Makefile
camlibs/ricoh/Makefile
camlibs/samsung/Makefile
camlibs/sierra/Makefile
camlibs/sipix/Makefile
camlibs/smal/Makefile
camlibs/sonix/Makefile
camlibs/sonydscf1/Makefile
camlibs/sonydscf55/Makefile
camlibs/soundvision/Makefile
camlibs/spca50x/Makefile
camlibs/sq905/Makefile
camlibs/stv0674/Makefile
camlibs/stv0680/Makefile
camlibs/sx330z/Makefile
camlibs/template/Makefile
camlibs/toshiba/Makefile
camlibs/toshiba/pdrm11/Makefile
camlibs/enigma13/Makefile
libgphoto2/Makefile
libgphoto2/libgphoto2.pc
tests/Makefile
tests/check-camera-list.sh
tests/ddb/Makefile
tests/ddb/check-ddb.sh
packaging/Makefile
packaging/linux-hotplug/Makefile
packaging/generic/Makefile
packaging/rpm/Makefile
packaging/rpm/package.spec
po/Makefile.in
doc/Makefile
doc/Doxyfile
doc/api/Makefile
bindings/Makefile
bindings/csharp/Makefile
bindings/csharp/AssemblyInfo.cs
bindings/csharp/libgphoto2-sharp.pc
bindings/csharp/check-camera-list.sh
bindings/csharp/check-sizes.sh
],[
dnl This relies on this code being called for each of the above files
dnl with ac_file set to the filename.
	if echo "$ac_file" | grep '.sh$' > /dev/null; then
		if test -f "$ac_file"; then
			chmod +x "$ac_file"
		fi
	fi
])
AC_OUTPUT

GP_CAMLIBS_CHECK_SUBDIRS

if test "x$CDPATH" != "x"
then
        echo
        echo "***********************************************************"
        echo "* You have CDPATH set. You may have to run \"unset CDPATH\" *"
        echo "* before running \"make dist\" or \"make rpm\".               *"
        echo "***********************************************************"
fi

GP_CONFIG_OUTPUT

echo "
Please check whether the configuration I detected matches what you
would like to have. E.g. make sure that USB support is there if you
intend to use USB cameras with $PACKAGE_NAME.

Please also check that PKG_CONFIG_PATH contains
    ${pkgconfigdir} 
before compiling any libgphoto2 frontend.
"
