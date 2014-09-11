dnl Process this file with autoreconf to produce a configure script.
AC_PREREQ(2.59)
AC_INIT([libgphoto2 port access library], [0.12.0], [gphoto-devel@lists.sourceforge.net], [libgphoto2_port])
AC_CONFIG_SRCDIR([libgphoto2_port/gphoto2-port-version.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([auto-m4])
AC_CONFIG_AUX_DIR([auto-aux])
dnl Working around bug in automake <= 1.9.6:
dnl  - Please do not add filename-length-max=99 here.
dnl  - Otherwise "make distdir" will fail in libgphoto2
dnl The generated Makefile rule fails to remove the absolute part at
dnl the beginning of the /path/to/buildroot/PACKAGE-VERSION/foo/bar
dnl before determining the string length. However, the only relevant
dnl string to determine the length of would be PACKAGE-VERSION/foo/bar
AM_INIT_AUTOMAKE([-Wall gnu 1.9 dist-bzip2 check-news])

AC_LANG(C)

# Use the silent-rules feature when possible.
m4_ifndef([AM_SILENT_RULES], [m4_define([AM_SILENT_RULES],[])])
AM_SILENT_RULES([no])

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
dnl A:R:C=6:1:6 libgphoto2_port-0.6.1 libgphoto2-2.2.1
dnl A:R:C=7:0:7 libgphoto2_port-0.7.0 libgphoto2-2.3
dnl A:R:C=7:1:7 libgphoto2_port-0.7.1 libgphoto2-2.3.x
dnl A:R:C=8:0:8 libgphoto2_port-0.8.0 libgphoto2-2.4.x
dnl A:R:C=9:1:9 libgphoto2_port-0.10.0 libgphoto2-2.5.x
AC_SUBST([LIBGPHOTO2_PORT_AGE],      [0])
AC_SUBST([LIBGPHOTO2_PORT_REVISION], [0])
AC_SUBST([LIBGPHOTO2_PORT_CURRENT],  [12])
AC_SUBST([LIBGPHOTO2_PORT_CURRENT_MIN],
         [`expr $LIBGPHOTO2_PORT_CURRENT - $LIBGPHOTO2_PORT_AGE`])
AC_SUBST([LIBGPHOTO2_PORT_VERSION_INFO],
         [$LIBGPHOTO2_PORT_CURRENT:$LIBGPHOTO2_PORT_REVISION:$LIBGPHOTO2_PORT_AGE])


dnl ---------------------------------------------------------------------------
dnl Checks for programs.
dnl ---------------------------------------------------------------------------
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AM_PROG_CC_C_O
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])


dnl Something with the sequences is not quite alright yet.
dnl SED is supposed to be set in AC_LIBTOOL_SETUP, but the
dnl sequence seems to get mixed up.
SED=${SED-sed}

dnl check for/set up libtool and libltdl
dnl AC_DISABLE_STATIC
dnl AC_DISABLE_SHARED
dnl AC_LIBLTDL_CONVENIENCE([libltdl])
dnl AC_WITH_LTDL
dnl AC_LIB_LTDL
dnl Disable building static library, as no one uses it anyway.
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
GP_CONFIG_MSG([Build])
GP_CONFIG_MSG([Source directory],[${srcdir}])
GP_CONFIG_MSG([Compiler],[${CC}])
GP_CONFIG_MSG([libltdl includes],[$LTDLINCL])
GP_CONFIG_MSG([libltdl library],[$LIBLTDL])

AC_DEFINE_UNQUOTED(HAVE_CC,"$CC",[The C compiler we're using])

AM_CPPFLAGS=""

# ----------------------------------------------------------------------
# References to external resources (web sites, mailing listes, etc.)
# ----------------------------------------------------------------------

GP_REFERENCES()dnl


dnl ---------------------------------------------------------------------------
dnl i18n support
dnl ---------------------------------------------------------------------------

dnl The following is a hack to get gphoto2 to build on gettext-0.10.35
dnl systems - BUILD_INCLUDED_LIBINTL is set only in gettext-0.10.37 which is
dnl the gettext version provided with gphoto2.
dnl BUILD_INCLUDED_LIBINTL=no
dnl AC_SUBST(BUILD_INCLUDED_LIBINTL)
dnl USE_INCLUDED_LIBINTL=no
dnl AC_SUBST(USE_INCLUDED_LIBINTL)

GP_GETTEXT_HACK([${PACKAGE}-${LIBGPHOTO2_PORT_CURRENT_MIN}],[Lutz Mueller and others],[${MAIL_GPHOTO_TRANSLATION}])
ALL_LINGUAS="cs da de es eu fi fr it ja nl pl pt_BR ru sk sr sv uk vi zh_CN zh_TW"
AM_GNU_GETTEXT_VERSION([0.14.1])
AM_GNU_GETTEXT([external])
AM_PO_SUBDIRS()
AM_ICONV()
GP_GETTEXT_FLAGS()

AC_SUBST([localedir],["\$(datadir)/locale"])
AM_CPPFLAGS="$AM_CPPFLAGS -DLOCALEDIR=\\\"${localedir}\\\""

AC_CHECK_FUNC(gettext, gettext_without_libintl=true)
# same trick as with libdl in libgphoto2:
# if gettext() doesn't require linking against libintl,
# we don't have to check for gettext in libintl. Otherwise
# we may even require libintl.
dnl AC_CHECK_LIB(intl, gettext, [LIBS="$LIBS -lintl"])
if test "$gettext_without_libintl" != "true" -a "$USE_NLS" = "yes"; then
        AC_CHECK_LIB(intl, gettext, [INTLLIBS="$INTLLIBS -lintl"])
fi


dnl ---------------------------------------------------------------------------
dnl Checks for header files.
dnl ---------------------------------------------------------------------------
AC_HEADER_DIRENT
AC_HEADER_STDC

AC_C_INLINE([])
AC_C_CONST([])

AC_CHECK_HEADERS(stdlib.h unistd.h stdio.h fcntl.h errno.h sys/time.h	\
	sys/param.h sys/select.h termios.h sgetty.h ttold.h ioctl-types.h	\
	fcntl.h sgtty.h sys/ioctl.h sys/time.h termio.h unistd.h	\
	endian.h byteswap.h asm/io.h mntent.h sys/mntent.h sys/mnttab.h \
	scsi/sg.h limits.h sys/file.h)
	
dnl FIXME: Provide regex.h with the corresponding object code for 
dnl        platforms which do not have it, e.g. Windows.
AC_CHECK_HEADER([regex.h],
                [AC_DEFINE([HAVE_REGEX],1,[whether we have regex.h])],[])
AC_CHECK_LIB([regex],[regexec])

dnl ---------------------------------------------------------------------------
dnl Apple Bonjour / mDNSResponder multicast DNS queries.
dnl ---------------------------------------------------------------------------
AC_ARG_WITH([bonjour],
	AS_HELP_STRING([--without-bonjour], [whether to exclude Bonjour PTP/IP autodetection]),
	,with_bonjour=yes)

MDNS_LIBS=
if test "x$with_bonjour" = "xyes"; then
	AC_CHECK_HEADER([dns_sd.h],[
		AC_CHECK_LIB(dns_sd,DNSServiceResolve,[
			MDNS_LIBS="-ldns_sd"
			AC_DEFINE(HAVE_MDNS_BONJOUR,1,[Define if you have Bonjour/mDNSResponder.])
		])
	])
fi
AC_SUBST(MDNS_LIBS)

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
	VERSIONMAPLDFLAGS="-Wl,--version-script=\$(srcdir)/libgphoto2_port.ver"
	AC_MSG_RESULT(yes)
],[
	VERSIONMAPLDFLAGS=""
	AC_MSG_RESULT(no)
])
AC_SUBST(VERSIONMAPLDFLAGS)

dnl Check for mnt_mountp in struct mnttab
AC_MSG_CHECKING([for mnt_mountp in struct mnttab])
AC_COMPILE_IFELSE([dnl
AC_LANG_PROGRAM([[
#if defined(HAVE_SYS_MNTENT_H) && defined(HAVE_SYS_MNTTAB_H)
# include <sys/mntent.h>
# include <sys/mnttab.h>
#endif
]], [[
struct mnttab mt;
mt.mnt_mountp;
]])dnl
],[
AC_DEFINE(HAVE_MNTTAB,1,whether struct mnttag has mnt_mountp field)
AC_MSG_RESULT(yes)
],[
AC_MSG_RESULT(no)
])



dnl Checks for library functions.
AC_CHECK_FUNCS(setmntent endmntent strerror snprintf vsnprintf flock)

dnl Check if TIOCM_RTS is included in one of several possible files
AC_TRY_COMPILE([#include <termios.h>], [int foo = TIOCM_RTS;],
			AC_DEFINE(HAVE_RTS_IOCTL,1,[Define if you have TIOCM_RTS.]))
AC_TRY_COMPILE([#include <termio.h>], [int foo = TIOCM_RTS;],
			AC_DEFINE(HAVE_RTS_IOCTL,1,[Define if you have TIOCM_RTS.]))
AC_TRY_COMPILE([#include <ioctl-types.h>], [int foo = TIOCM_RTS;],
			AC_DEFINE(HAVE_RTS_IOCTL,1,[Define if you have TIOCM_TRS.]))
AC_TRY_COMPILE([#include <sys/ioctl.h>], [int foo = TIOCM_RTS;],
			AC_DEFINE(HAVE_RTS_IOCTL,1,[Define if you have TIOCM_TRS.]))

# Check for va_copy()
GP_VA_COPY


dnl ---------------------------------------------------------------------------
dnl serial
dnl ---------------------------------------------------------------------------
serial_msg=yes
have_serial=true
try_ttylock=true
try_lockdev=true
try_resmgr=true

if test "$ac_cv_header_sgtty_h" = "no"
then
  have_serial=false
  serial_msg=no
fi

AC_ARG_ENABLE([serial],
	[AS_HELP_STRING([--disable-serial],
			[do not compile in SERIAL support])],
[
	if test x$enableval = xno; then
		have_serial=false
		serial_msg=no
		try_ttylock=false
		try_lockdev=false
		try_resmgr=false
	fi
])
if $have_serial; then
	IOLIB_LIST="$IOLIB_LIST serial"
	AC_DEFINE([HAVE_SERIAL],1,[Whether you have serial support enabled])
fi
GP_CONFIG_MSG([Serial ports])
GP_CONFIG_MSG([Serial support],[$serial_msg])

SERIAL_LIBS=""
if $have_serial; then

dnl -----------------------------------------
dnl Serial port locking: We try to use either
dnl  (1) ttylock.h
dnl  (2) lockdev.h
dnl -----------------------------------------
AC_DEFUN([GP_SERLOCK],[dnl
# $0([$1])
AC_ARG_ENABLE([$1],
[AS_HELP_STRING([--disable-][$1],
		[do not use ][$1][ library])
],[
	if test x$enableval = xno; then
		try_[$1]=false
	fi
])
])dnl
GP_SERLOCK([ttylock])dnl
GP_SERLOCK([lockdev])dnl

ttylock_msg=no
if $try_ttylock; then
	AC_CHECK_HEADER([lockdev.h])
	AC_CHECK_HEADER([ttylock.h],[
		lockdev_result="no"
		AC_TRY_LINK([#include <ttylock.h>],[ttylocked ("/dev/foo");],[
			lockdev_result="no"
			AC_DEFINE(HAVE_TTYLOCK,1,[Define if you have ttylock based tty locking.])
			ttylock_msg=yes
			try_lockdev=false,
			save_LIBS="$LIBS"
			LIBS="$LIBS -llockdev"
			AC_TRY_LINK([#include <ttylock.h>],[ttylocked ("/dev/foo");],[
				lockdev_result="yes"
				AC_DEFINE(HAVE_TTYLOCK,1,[Define if you have ttylock based tty locking.])
				ttylock_msg=yes
				try_lockdev=false
				SERIAL_LIBS=-llockdev
			])
			LIBS="$save_LIBS"
		])
		AC_MSG_CHECKING([if ttylock needs lockdev])
		AC_MSG_RESULT([${lockdev_result}])
	],[],[#ifdef HAVE_LOCKDEV_H
# include <lockdev.h>
#endif
	])
fi
GP_CONFIG_MSG([ttylock locking],[${ttylock_msg}])

lockdev_msg=no
if $try_lockdev; then
	AC_CHECK_LIB(lockdev, dev_lock,[
		AC_CHECK_HEADER(lockdev.h,[
			AC_DEFINE(HAVE_LOCKDEV,1,[Define if you have dev_lock/lockdev based locking.])
			lockdev_msg=yes
			SERIAL_LIBS=-llockdev
		])
	])
fi
GP_CONFIG_MSG([lockdev locking],[${lockdev_msg}])

fi # have_serial

AC_SUBST([SERIAL_LIBS])


dnl ---------------------------------------------------------------------------
dnl libusb
dnl ---------------------------------------------------------------------------

dnl libpthread is dynamically linked by libusb, which is dynamically loaded
dnl but newer Linux systems need it on startup, not later. So link libgphoto2_port
dnl against libpthread
AC_CHECK_LIB(pthread, pthread_create)

GP_CONFIG_MSG([USB ports])
GP_CHECK_LIBRARY([LIBUSB],[libusb],[>= 0.1.5],
		[usb.h],[usb_open],[
        case "$host" in
          *-*-mingw* | *-*-cygwin* | *-*-msvc* )
          dnl Windows itself has usb.h we mis-detect so don't set IOLIB_LIST
          ;;
          *)
	  IOLIB_LIST="$IOLIB_LIST usb usbdiskdirect usbscsi"
          ;;
        esac
],[],
		[default-on],
		[http://libusb.sourceforge.net/]
)

dnl ---------------------------------------------------------------------------
dnl libusb1
dnl ---------------------------------------------------------------------------
GP_CONFIG_MSG([USB ports])
GP_CHECK_LIBRARY([LIBUSB1],[libusb-1.0],[>= 1.0.0],
		[libusb.h],[libusb_init],[
        case "$host" in
          *-*-mingw* | *-*-cygwin* | *-*-msvc* )
	  IOLIB_LIST="$IOLIB_LIST usb1"
          ;;
          *)
	  IOLIB_LIST="$IOLIB_LIST usb1 usbdiskdirect usbscsi"
          ;;
        esac

	# only in libusbx? but not in traditional libusb 1.0
	AC_CHECK_LIB(usb-1.0,libusb_strerror, [
		AC_DEFINE(HAVE_LIBUSB_STRERROR,1,[Define if libusb-1.0 has libusb_strerror])
])

],[],
		[default-on],
		[http://libusb.sourceforge.net/]
)
dnl depends on libusb's availability
GP_UDEV([test "x$have_LIBUSB" = xyes -o "x$have_LIBUSB1" = xyes])


AC_ARG_ENABLE([disk],
	AS_HELP_STRING([--disable-disk], [disable the 'disk' port driver]),
	,enable_disk=yes)

dnl disk port also works everywhere, but it's not really necessary
if test "x$enable_disk" = "xyes"; then
	IOLIB_LIST="$IOLIB_LIST disk"
fi

AC_ARG_ENABLE([ptpip],
	AS_HELP_STRING([--disable-ptpip], [disable the 'ptpip' port driver for TCP/IP connected PTP cameras]),
	,enable_ptpip=yes)
dnl ptpip - works 'everywhere' with TCP/IP stack.
if test "x$enable_ptpip" = "xyes"; then
	IOLIB_LIST="$IOLIB_LIST ptpip"
fi

# ----------------------------------------------------------------------
# Define IOLIB stuff
# ----------------------------------------------------------------------

AC_SUBST(IOLIB_LIST)
for x in ${IOLIB_LIST}; do
    IOLIB_LTLIST="${IOLIB_LTLIST} ${x}.la"
done
AC_SUBST(IOLIB_LTLIST)
AC_SUBST([iolibdir],["\$(libdir)/\$(PACKAGE_TARNAME)/\$(VERSION)"])
AM_CPPFLAGS="$AM_CPPFLAGS -DIOLIBS=\\\"${iolibdir}\\\""


dnl --------------------------------------------------------------------
dnl guess directory to install *.pc into
dnl --------------------------------------------------------------------
pkgconfigdir='${libdir}/pkgconfig'
AC_SUBST([pkgconfigdir])


dnl ----------------------------------------------------------------------
dnl GNU regex?
dnl ----------------------------------------------------------------------
AC_CHECK_FUNC(re_compile_pattern,
              AC_DEFINE(HAVE_GNU_REGEX,1,[Define if you have GNU regex.])
)


dnl ----------------------------------------------------------------------
dnl disable debugging if requested
dnl ----------------------------------------------------------------------
AC_ARG_ENABLE([debug],
  AS_HELP_STRING([--disable-debug],[do not include debugging code]),
[
  if test "$enableval" = "no"; then
    AC_DEFINE(DISABLE_DEBUGGING,1,[Define if you want to disabled debugging.])
  fi
])


# ----------------------------------------------------------------------
# gtk-doc?
# ----------------------------------------------------------------------
GP_CONFIG_MSG([Documentation])
GP_CHECK_DOC_DIR()dnl
GP_BUILD_GTK_DOCS()dnl


dnl ---------------------------------------------------------------------------
dnl Turn on all warnings when using gcc
dnl ---------------------------------------------------------------------------
if test "$GCC" = "yes"; then
	CFLAGS="$CFLAGS -Wall"
	CFLAGS="$CFLAGS -Wmissing-declarations -Wmissing-prototypes"
fi

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


# ---------------------------------------------------------------------------
# Create output files
# ---------------------------------------------------------------------------
AC_CONFIG_FILES([
Makefile
po/Makefile.in
libgphoto2_port/Makefile
libgphoto2_port.pc
libgphoto2_port-uninstalled.pc
gphoto2-port-config
test/Makefile
doc/Makefile
m4/Makefile
])
AC_OUTPUT

GP_CONFIG_OUTPUT

# End of file.
