dnl Process this file with autoreconf to produce a configure script.
AC_PREREQ(2.59)

dnl NOTE: Be version style _higher_ than the last release.
dnl       So lastversion.X.trunk for instance. Bump X if necessary.

dnl ******* THE NEXT RELEASE VERSION MUST BE 2.5.7 at least or higher!
AC_INIT([libgphoto2 photo camera library], [2.5.7], [gphoto-devel@lists.sourceforge.net], [libgphoto2])

AC_CONFIG_SRCDIR([libgphoto2/gphoto2-version.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([auto-m4])
AC_CONFIG_AUX_DIR([auto-aux])
dnl Working around bug in automake <= 1.9.6:
dnl  - Please do not add filename-length-max=99 here.
dnl  - Otherwise "make distdir" will fail in gphoto-suite
dnl The generated Makefile rule fails to remove the absolute part at
dnl the beginning of the /path/to/buildroot/PACKAGE-VERSION/foo/bar
dnl before determining the string length. However, the only relevant
dnl string to determine the length of would be PACKAGE-VERSION/foo/bar
AM_INIT_AUTOMAKE([-Wall foreign 1.9 dist-bzip2 check-news subdir-objects])


# Use the silent-rules feature when possible.
m4_ifndef([AM_SILENT_RULES], [m4_define([AM_SILENT_RULES],[])])
AM_SILENT_RULES([no])

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
dnl  - AGE (Micro):      Increment if any interfaces have been added; set to 0
dnl		         if any interfaces have been removed. Removal has 
dnl                      precedence over adding, so set to 0 if both happened.
dnl                      It denotes upward compatibility.
dnl  - REVISION (Minor): Increment any time the source changes; set to 
dnl			 0 if you incremented CURRENT.
dnl  - CURRENT (Major):  Increment if the interface has changes. AGE is always
dnl                      *changed* at the same time.
dnl
dnl  To summarize. Any interface *change* increment CURRENT. If that interface
dnl  change does not break upward compatibility (ie it is an addition), 
dnl  increment AGE, Otherwise AGE is reset to 0. If CURRENT has changed, 
dnl  REVISION is set to 0, otherwise REVISION is incremented.
dnl ---------------------------------------------------------------------------
dnl A:R:C=1:1:3  libgphoto2-2.2
dnl A:R:C=2:0:4  libgphoto2-2.3 (since libgphoto2-2.2.1.5.trunk)
dnl A:R:C=2:1:4  libgphoto2-2.3.1 (since libgphoto2-2.3.0.1.trunk)
dnl A:R:C=3:0:5  libgphoto2-2.3.0.3.trunk (new gp_file_new_from_fd() function)
dnl              libgphoto2-2.3.1.1.trunk (new gp_file_slurp() function)
dnl              libgphoto2-2.3.1.2.trunk (new gp_camera_get_storageinfo() function + enums + struct)
dnl A:R:C=0:0:6  libgphoto2-2.5.0	(fixed CameraList API, ... )
AC_SUBST([LIBGPHOTO2_AGE],      [0])
AC_SUBST([LIBGPHOTO2_REVISION], [0])
AC_SUBST([LIBGPHOTO2_CURRENT],  [6])
AC_SUBST([LIBGPHOTO2_CURRENT_MIN],
         [`expr $LIBGPHOTO2_CURRENT - $LIBGPHOTO2_AGE`])
AC_SUBST([LIBGPHOTO2_VERSION_INFO],
         [$LIBGPHOTO2_CURRENT:$LIBGPHOTO2_REVISION:$LIBGPHOTO2_AGE])


dnl ---------------------------------------------------------------------------
dnl Checks for programs.
dnl ---------------------------------------------------------------------------

AC_PROG_CC
AC_PROG_CPP
AC_PROG_CXX
AC_PROG_INSTALL
AM_PROG_CC_C_O
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

dnl Something with the sequences is not quite alright yet.
dnl SED is supposed to be set in AC_LIBTOOL_SETUP, but the
dnl sequence seems to get mixed up.
SED=${SED-sed}

dnl ---------------------------------------------------------------------------
dnl Turn on (almost) all warnings when using gcc
dnl ---------------------------------------------------------------------------
if test "x$GCC" = "xyes"; then
	# CFLAGS="$CFLAGS --std=c99 -Wall -Wextra -Werror -pedantic"
	CFLAGS="$CFLAGS -Wall -Wmissing-declarations -Wmissing-prototypes"
	LDFLAGS="$LDFLAGS"
	AC_SUBST([NO_UNUSED_CFLAGS], [-Wno-unused])
fi
AM_CONDITIONAL([HAVE_GCC], [test "x$GCC" = "xyes"])

# Activate internal code
AM_CPPFLAGS="$AM_CPPFLAGS -D_GPHOTO2_INTERNAL_CODE"

# CPPFLAGS: C PreProcessor flags
AC_SUBST([AM_CPPFLAGS])
# CFLAGS: C compiler flags
AC_SUBST([AM_CFLAGS])
# CXXFLAGS: C++ compiler flags
AC_SUBST([AM_CXXFLAGS])
# LDFLAGS: Linker flags
AC_SUBST([AM_LDFLAGS])

# Check for va_copy()
GP_VA_COPY

# Note: Do NOT add specific libraries and stuff to the generic _FLAGS.
#       Put these into LIBFOO_CFLAGS and LIBFOO_LIBS instead.


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

AM_CONDITIONAL([ENABLED_GP2DDB], ["$try_gp2ddb" && test -x "$FLEX" && test -x "$BISON"])

if "$try_gp2ddb" && test -x "$FLEX" && test -x "$BISON"; then
	gp_msg_gp2ddb="yes $msg_gp2ddb"
        AC_DEFINE([ENABLED_GP2DDB], [1], [Whether we have flex and bison for GP2DDB])
else
	gp_msg_gp2ddb="no $msg_gp2ddb"
fi
GP_CONFIG_MSG([GP2DDB support], [${gp_msg_gp2ddb} (EXPERIMENTAL)])

GP_PKG_CONFIG


dnl check for/set up libtool and libltdl
dnl AC_DISABLE_STATIC
dnl AC_DISABLE_SHARED
dnl AC_LIBLTDL_CONVENIENCE([libltdl])
dnl AC_WITH_LTDL
dnl AC_LIB_LTDL
AC_DISABLE_STATIC 
AC_LIBTOOL_WIN32_DLL
AC_LIBTOOL_DLOPEN
AC_PROG_LIBTOOL

dnl We are using our own libltdl checks instead of AC_WITH_LTDL
dnl because we do not want to ship our own copy of libltdl any more.
dnl Not shipping libltdl makes it possible to ditch our own autogen.sh
dnl and relying on standard autoconf's "autoreconf".
GP_LIB_LTDL


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

ALL_LINGUAS="cs da de es eu fr hu it ja nl pl ru sv uk vi zh_CN"
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

AC_SYS_LARGEFILE

AC_MSG_CHECKING([for asm .symver support])
AC_COMPILE_IFELSE([dnl
	AC_LANG_PROGRAM([[]],[[
		int f1() { }
		int f2() { }
		asm(".symver f1, f@VER1");
		asm(".symver f2, f@@VER2");
		int main(int argc, char **argv) { }
	]])dnl
],[
	AC_DEFINE([HAVE_ASM_SYMVERS],1,[Define if there is asm .symver support.])
	VERSIONMAPLDFLAGS="-Wl,--version-script=\$(srcdir)/libgphoto2.ver"
	AC_MSG_RESULT(yes)
],[
	VERSIONMAPLDFLAGS=""
	AC_MSG_RESULT(no)
])
AC_SUBST(VERSIONMAPLDFLAGS)

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
AC_ARG_WITH([jpeg], AS_HELP_STRING([--without-jpeg], [Build without jpeg library (default: no)]))
AS_IF([test "x$with_jpeg" != "xno"], [
	AC_CHECK_LIB(jpeg,jpeg_start_decompress,[
		AC_CHECK_HEADER(jpeglib.h,[
			AC_DEFINE(HAVE_LIBJPEG,1,[define if we found LIBJPEG and its headers])
			LIBJPEG="-ljpeg"
			libjpeg_msg="yes"
		])
	])
])
GP_CONFIG_MSG([JPEG mangling support],[${libjpeg_msg}])

dnl ---------------------------------------------------------------------------
dnl check for libxml2
dnl ---------------------------------------------------------------------------
GP_CHECK_LIBRARY([LIBXML2],[libxml-2.0],[>= 2.0],
                [libxml/parser.h],[xmlFirstElementChild],[],[],
                [default-on],
                [http://xmlsoft.org]
)
GP_CONFIG_MSG([XML support for Olympus E series],[${have_LIBXML2}])

dnl ---------------------------------------------------------------------------
dnl check for libgd
dnl they call it "gdlib" ... confusing.
dnl ---------------------------------------------------------------------------
GP_CHECK_LIBRARY([LIBGD],[gdlib],[>= 2.0],[gd.h],[gdImageCreateTrueColor],[
	# gdlib-config --libs upstream does not include -lgd, but only the dependencies
	LIBGD_LIBS="-lgd $LIBGD_LIBS"

],[],[default-on],[http://www.libgd.org/])
GP_CONFIG_MSG([libGD conversion support],[${have_LIBGD}])

dnl ---------------------------------------------------------------------------
dnl Checks for header files.
dnl ---------------------------------------------------------------------------
AC_HEADER_DIRENT
# before _HEADER_STDC
AC_HEADER_STDC
# after _HEADER_STDC
AC_CHECK_HEADERS([sys/param.h sys/mman.h sys/select.h locale.h memory.h getopt.h unistd.h mcheck.h limits.h sys/time.h langinfo.h])
AC_C_INLINE([])
AC_C_CONST([])
dnl FIXME: AC_STRUCT_TIMEZONE


dnl ---------------------------------------------------------------------------
dnl more header/function/def checks
dnl ---------------------------------------------------------------------------

dnl Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

dnl Checks for library functions.
AC_CHECK_FUNCS([getopt getopt_long mkdir strdup strncpy strcpy snprintf sprintf vsnprintf gmtime_r statfs localtime_r lstat inet_aton rand_r])

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

AC_CHECK_HEADERS([sys/mount.h sys/statfs.h sys/user.h sys/vfs.h],,,[
#include <sys/types.h>
#if HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
])

dnl Check for statfs members
AC_CHECK_MEMBERS([struct statfs.f_bfree, struct statfs.f_bavail, struct statfs.f_favail],,,[
#include <sys/types.h>
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
#ifdef HAVE_SYS_MOUNT_H
# include <sys/mount.h>
#endif
#ifdef HAVE_SYS_VFS_H
# include <sys/vfs.h>
#endif
#ifdef HAVE_SYS_STATFS_H
# include <sys/statfs.h>
#endif
])

AC_CHECK_MEMBERS([struct statvfs.f_blocks],,,[
#ifdef HAVE_SYS_STATVFS_H
# include <sys/statvfs.h>
#endif
])


dnl ---------------------------------------------------------------------------
dnl Checks for integer types.
dnl ---------------------------------------------------------------------------
dnl
dnl FIXME: Test AC_TYPE_UINT32_T & Co and check whether that could work for us.
dnl        Potential problem: The required autoconf version.
dnl Note: We are already using AC_TYPE_SIZE_T somewhere else in here.


dnl Create a stdint.h-like file containing size-specific integer definitions
dnl that will always be available
AX_NEED_STDINT_H([libgphoto2/_stdint.h])

dnl Create a header file containing NetBSD-style byte swapping macros
AC_NEED_BYTEORDER_H([libgphoto2/gphoto2-endian.h])


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

if test "x${utilsdir}" = "x"; then utilsdir="\${libdir}/\${PACKAGE_TARNAME}"; fi
AC_ARG_VAR([utilsdir],[Directory where utilities like print-camera-list will be installed])
AC_SUBST([utilsdir])

GP_UDEV([true])


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


dnl ---------------------------------------------------------------------------
dnl Camera drivers (camlibs)
dnl ---------------------------------------------------------------------------
dnl * See m4m/gp-camlibs.m4 for usage details, e.g. for marking camlibs as
dnl   unlisted.
dnl * Please add new camlibs in alphabetical order.
dnl * Selective build:
dnl   $ make -C camlibs canon.la ptp2.la
dnl * Selective install:
dnl   $ make CAMLIBS="canon.la ptp2.la" install-camlibs
GP_CAMLIB([adc65])dnl
GP_CAMLIB([agfa_cl20])dnl
GP_CAMLIB([aox])dnl
GP_CAMLIB([ax203])dnl
GP_CAMLIB([barbie])dnl
GP_CAMLIB([canon])dnl
GP_CAMLIB([casio_qv])dnl
GP_CAMLIB([clicksmart310])dnl
GP_CAMLIB([digigr8])dnl
GP_CAMLIB([digita])dnl
GP_CAMLIB([dimera3500])dnl
GP_CAMLIB([directory])dnl
GP_CAMLIB([enigma13])dnl
GP_CAMLIB([fuji])dnl
GP_CAMLIB([gsmart300])dnl
GP_CAMLIB([hp215])dnl
GP_CAMLIB([iclick])dnl
GP_CAMLIB([jamcam])dnl
GP_CAMLIB([jd11])dnl
GP_CAMLIB([jl2005a])dnl
GP_CAMLIB([jl2005c])dnl
GP_CAMLIB([kodak_dc120])dnl
GP_CAMLIB([kodak_dc210])dnl
GP_CAMLIB([kodak_dc240])dnl
GP_CAMLIB([kodak_dc3200])dnl
GP_CAMLIB([kodak_ez200])dnl
GP_CAMLIB([konica])dnl
GP_CAMLIB([konica_qm150])dnl
GP_CAMLIB([largan])dnl
GP_CAMLIB([lg_gsm])dnl
GP_CAMLIB([mars])dnl
GP_CAMLIB([dimagev])dnl
GP_CAMLIB([mustek])dnl
GP_CAMLIB([panasonic_coolshot])dnl
GP_CAMLIB([panasonic_l859])dnl
GP_CAMLIB([panasonic_dc1000])dnl
GP_CAMLIB([panasonic_dc1580])dnl
GP_CAMLIB([pccam300])dnl
GP_CAMLIB([pccam600])dnl
GP_CAMLIB([pentax])dnl
GP_CAMLIB([polaroid_pdc320])dnl
GP_CAMLIB([polaroid_pdc640])dnl
GP_CAMLIB([polaroid_pdc700])dnl
GP_CAMLIB([ptp2])dnl
GP_CAMLIB([ricoh])dnl
GP_CAMLIB([ricoh_g3])dnl
GP_CAMLIB([samsung])dnl
GP_CAMLIB([sierra])dnl
GP_CAMLIB([sipix_blink], [unlisted])dnl did not find out the correct decompressor, unfinished
GP_CAMLIB([sipix_blink2])dnl
GP_CAMLIB([sipix_web2])dnl
GP_CAMLIB([smal])dnl
GP_CAMLIB([sonix])dnl
GP_CAMLIB([sony_dscf1])dnl
GP_CAMLIB([sony_dscf55])dnl
GP_CAMLIB([soundvision])dnl
GP_CAMLIB([spca50x])dnl
GP_CAMLIB([sq905])dnl
GP_CAMLIB([st2205])dnl
GP_CAMLIB([stv0674])dnl
GP_CAMLIB([stv0680])dnl
GP_CAMLIB([sx330z])dnl
GP_CAMLIB([template], [unlisted])dnl
GP_CAMLIB([topfield])dnl
GP_CAMLIB([toshiba_pdrm11])dnl
GP_CAMLIB([tp6801])dnl
dnl
GP_CAMLIBS_DEFINE()dnl
GP_CONFIG_MSG([Camlibs],[${camlibs}])


dnl --------------------------------------------------------------------
dnl documentation
dnl --------------------------------------------------------------------
GP_CHECK_DOC_DIR()dnl
GP_CHECK_DOXYGEN()dnl
GP_INTERNAL_DOCS()dnl
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


dnl FIXME: Is this accurate?
AM_CONDITIONAL([HAVE_CXX], [test -n "$CXX" && test "X$CXX" != "Xno"])


dnl ---------------------------------------------------------------------------
dnl Configure subprojects
dnl ---------------------------------------------------------------------------
if test -d "$srcdir/libgphoto2_port"; then
	AC_CONFIG_SUBDIRS([libgphoto2_port])
else
	AC_MSG_ERROR([
*** Hey, where have you hidden my libgphoto2_port?
*** I needed that!
])
fi


# ---------------------------------------------------------------------------
# Create output files
# ---------------------------------------------------------------------------
AC_CONFIG_FILES([
README
Makefile
gphoto2-config
camlibs/Makefile
camlibs/canon/doc/Makefile
camlibs/konica/localization/Makefile
libgphoto2/Makefile
libgphoto2.pc
libgphoto2-uninstalled.pc
examples/Makefile
tests/Makefile
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
doc/Doxyfile-internals
doc/api/Makefile
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

if test "x$CDPATH" != "x"
then
        echo
        echo "***********************************************************"
        echo "* You have CDPATH set. You may have to run \"unset CDPATH\" *"
        echo "* before running \"make dist\" or \"make rpm\".               *"
        echo "***********************************************************"
fi

GP_CONFIG_OUTPUT

dnl Catch the Gentoo folks about to shoot themselves in the foot.
dnl Just enabling "foo" often does not make a camera made by
dnl manufacturer "Foo" work.
if test "x$camlibs" = "xall"; then
   :
else
   GP_CAMLIBS_WARNING
fi

echo "Please check whether the configuration I detected matches what you
would like to have. E.g. make sure that USB support is there if you
intend to use USB cameras with $PACKAGE_NAME.

Please also check that PKG_CONFIG_PATH contains
    ${pkgconfigdir} 
before compiling any libgphoto2 frontend.
"
