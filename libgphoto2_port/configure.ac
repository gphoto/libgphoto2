dnl Process this file with autoreconf to produce a configure script.
AC_PREREQ([2.69])

AC_INIT([libgphoto2 port access library],
        [0.12.2],
        [gphoto-devel@lists.sourceforge.net],
        [libgphoto2_port])

AC_CONFIG_SRCDIR([libgphoto2_port/gphoto2-port-version.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([auto-m4])
AC_CONFIG_AUX_DIR([auto-aux])
AM_INIT_AUTOMAKE([
  -Wall
  gnu
  1.14
  dist-bzip2
  check-news
  subdir-objects
])


AM_SILENT_RULES([yes])


dnl Every compile example will be using the C language unless
dnl something else is said explicitly.
AC_LANG([C])



dnl Flag all GP_ strings in result as error unless specifically allowed.
m4_pattern_forbid([^_?GP_])dnl


dnl ---------------------------------------------------------------------------
dnl Advanced information about versioning:
dnl   * "Writing shared libraries" by Mike Hearn
dnl         http://plan99.net/~mike/writing-shared-libraries.html
dnl         originally http://navi.cx/~mike/writing-shared-libraries.html
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
dnl A:R:C=1:0:13  libgphoto2-2.5.30	(added gp_port_init_localedir ... )
dnl A:R:C=2:0:14  libgphoto2-2.5.31	(added gpi_libltdl_mutex ... )
AC_SUBST([LIBGPHOTO2_PORT_AGE],      [2])
AC_SUBST([LIBGPHOTO2_PORT_REVISION], [0])
AC_SUBST([LIBGPHOTO2_PORT_CURRENT],  [14])
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
AC_PROG_SED
AM_PROG_AR


dnl check for/set up libtool and libltdl
m4_pattern_forbid([^LT_PREREQ])dnl
LT_PREREQ([2.4.2])
dnl Disable building static library, as no one uses it anyway.
m4_pattern_forbid([^LT_INIT])dnl
LT_INIT([
  disable-static
  dlopen
  win32-dll
])
dnl
dnl LTDL_INIT([external])
dnl
dnl We are using our own libltdl checks instead of LTDL_INIT because
dnl we do not want to ship our own two copies of libltdl (one for
dnl libgphoto2 and one for libgphoto2_port), and LTDL_INIT insists on
dnl that.
GP_LIBLTDL


# report on compiler/libtool setup
GP_CONFIG_MSG([Build])
GP_CONFIG_MSG([Source directory], [${srcdir}])
GP_CONFIG_MSG([Compiler],         [${CC}])
GP_CONFIG_MSG([libltdl includes], [$LTDLINCL])
GP_CONFIG_MSG([libltdl library],  [$LIBLTDL])

AC_DEFINE_UNQUOTED([HAVE_CC], ["$CC"],
                   [The C compiler we're using])

AM_CPPFLAGS=""

# ----------------------------------------------------------------------
# References to external resources (web sites, mailing listes, etc.)
# ----------------------------------------------------------------------

GP_REFERENCES()dnl


dnl ---------------------------------------------------------------------------
dnl i18n support
dnl ---------------------------------------------------------------------------

GP_GETTEXT_SETUP([GETTEXT_PACKAGE_LIBGPHOTO2_PORT],
                 [${PACKAGE}-${LIBGPHOTO2_PORT_CURRENT_MIN}],
                 [po])
AM_PO_SUBDIRS()
AM_GNU_GETTEXT_VERSION([0.19.1])
AM_GNU_GETTEXT([external])
AM_ICONV()

GP_CONFIG_MSG([Use translations],
              [${USE_NLS}])

AC_CHECK_FUNC([gettext], [gettext_without_libintl=true])
# same trick as with libdl in libgphoto2:
# if gettext() doesn't require linking against libintl,
# we don't have to check for gettext in libintl. Otherwise
# we may even require libintl.
dnl AC_CHECK_LIB(intl, gettext, [LIBS="$LIBS -lintl"])
AS_IF([test "x$gettext_without_libintl" != xtrue && test "x$USE_NLS" = xyes], [dnl
  AC_CHECK_LIB([intl], [gettext], [INTLLIBS="$INTLLIBS -lintl"])
])


dnl ---------------------------------------------------------------------------
dnl Checks for header files.
dnl ---------------------------------------------------------------------------
AC_HEADER_DIRENT
AC_HEADER_STDC

AC_C_INLINE([])
AC_C_CONST([])

AC_CHECK_HEADERS([m4_normalize([
  asm/io.h
  byteswap.h
  endian.h
  errno.h
  fcntl.h
  ioctl-types.h
  limits.h
  mntent.h
  scsi/sg.h
  sgetty.h
  sgtty.h
  stdio.h
  stdlib.h
  sys/file.h
  sys/ioctl.h
  sys/mntent.h
  sys/mnttab.h
  sys/param.h
  sys/select.h
  sys/time.h
  termio.h
  termios.h
  ttold.h
  unistd.h
])])

dnl FIXME: Provide regex.h with the corresponding object code for
dnl        platforms which do not have it, e.g. Windows.
AC_CHECK_HEADER([regex.h],
                [AC_DEFINE([HAVE_REGEX], [1],
		           [whether we have regex.h])],
		[])
AC_CHECK_LIB([regex], [regexec])

GP_ASM_SYMVER_SUPPORT

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
], [dnl
  AC_DEFINE([HAVE_MNTTAB], [1],
            [whether struct mnttag has mnt_mountp field])
  AC_MSG_RESULT([yes])
], [dnl
  AC_MSG_RESULT([no])
])


dnl Checks for library functions.
AC_CHECK_FUNCS([m4_normalize([
  endmntent
  flock
  setmntent
  snprintf
  strerror
  vsnprintf
])])

dnl Check if TIOCM_RTS is included in one of several possible files
gp_found_tiocm_rts=no
AC_DEFUN([GP_TRY_TIOCM_RTS], [dnl
  AS_VAR_IF([gp_found_tiocm_rts], [no], [dnl
    AC_COMPILE_IFELSE([dnl
      AC_LANG_PROGRAM([[#include <$1>]], [[
        int foo = TIOCM_RTS;
      ]])
    ], [dnl
      gp_found_tiocm_rts=yes
    ])
  ])
])dnl
GP_TRY_TIOCM_RTS([termios.h])
GP_TRY_TIOCM_RTS([termio.h])
GP_TRY_TIOCM_RTS([ioctl-types.h])
GP_TRY_TIOCM_RTS([sys/ioctl.h])
AS_VAR_IF([gp_found_tiocm_rts], [yes], [dnl
  AC_DEFINE([HAVE_RTS_IOCTL], [1],
            [Define if you have TIOCM_TRS.])
])

# Check for va_copy()
GP_VA_COPY

dnl ---------------------------------------------------------------------------
dnl libexif: The virtual usb camera driver can use libexif for extracting thumbnails
dnl	     out of EXIF data. Similarly, it can extract the mtime of
dnl	     a file.
dnl	     libexif is available from
dnl	     http://www.sourceforge.net/projects/libexif
dnl ---------------------------------------------------------------------------
GP_CHECK_LIBRARY([LIBEXIF], [libexif], [>= 0.6.13],
                 [libexif/exif-data.h], [exif_data_new], [dnl
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
]])], [dnl
   AC_DEFINE([HAVE_LIBEXIF_IFD], [1],
	     [whether we use a version of libexif with ExifData.ifd[[]]])
   AC_MSG_RESULT([yes])
], [dnl
   AC_MSG_RESULT([no])
])
CPPFLAGS="$CPPFLAGS_save"
], [], [default-on], [https://github.com/libexif/libexif])dnl



dnl ---------------------------------------------------------------------------
dnl serial
dnl ---------------------------------------------------------------------------
serial_msg=yes
have_serial=true
try_ttylock=true
try_lockdev=true
try_resmgr=true

AS_VAR_IF([ac_cv_header_sgtty_h], [no], [dnl
  have_serial=false
  serial_msg=no
])

AC_ARG_ENABLE([serial],
	[AS_HELP_STRING([--disable-serial],
			[do not compile in SERIAL support])],
[dnl
	AS_VAR_IF([enableval], [no], [dnl
		have_serial=false
		serial_msg=no
		try_ttylock=false
		try_lockdev=false
		try_resmgr=false
	])
])
AS_IF([$have_serial], [dnl
	IOLIB_LIST="$IOLIB_LIST serial"
	AC_DEFINE([HAVE_SERIAL], [1],
	          [Whether you have serial support enabled])
])
GP_CONFIG_MSG([Serial ports])
GP_CONFIG_MSG([Serial support], [$serial_msg])

dnl -----------------------------------------
dnl Serial port locking: We try to use either
dnl  (1) ttylock.h
dnl  (2) lockdev.h
dnl -----------------------------------------
AC_DEFUN([GP_SERLOCK],[dnl
# $0([$1])
AC_ARG_ENABLE([$1],
[AS_HELP_STRING([--disable-][$1],
                [do not use ][$1][ library])dnl
], [dnl
  AS_VAR_IF([enableval], [no], [dnl
    try_[$1]=false
  ])
])
])dnl

SERIAL_LIBS=""
AS_IF([$have_serial], [dnl

GP_SERLOCK([ttylock])dnl
GP_SERLOCK([lockdev])dnl

ttylock_msg=no
AS_IF([$try_ttylock], [dnl
	AC_CHECK_HEADER([lockdev.h])
	AC_CHECK_HEADER([ttylock.h], [dnl
		lockdev_result="no"
		AC_LINK_IFELSE([dnl
			AC_LANG_PROGRAM([[#include <ttylock.h>
			                ]], [[ttylocked("/dev/foo");]])
		], [dnl
			lockdev_result="no"
			AC_DEFINE([HAVE_TTYLOCK], [1],
			          [Define if you have ttylock based tty locking.])
			ttylock_msg=yes
			try_lockdev=false,
			save_LIBS="$LIBS"
			LIBS="$LIBS -llockdev"
			AC_LINK_IFELSE([dnl
				AC_LANG_PROGRAM([[#include <ttylock.h>
				                ]], [[ttylocked ("/dev/foo");]])
			], [dnl
				lockdev_result="yes"
				AC_DEFINE([HAVE_TTYLOCK], [1],
				          [Define if you have ttylock based tty locking.])
				ttylock_msg=yes
				try_lockdev=false
				SERIAL_LIBS=-llockdev
			])
			LIBS="$save_LIBS"
		])
		AC_MSG_CHECKING([if ttylock needs lockdev])
		AC_MSG_RESULT([${lockdev_result}])
	], [], [#ifdef HAVE_LOCKDEV_H
# include <lockdev.h>
#endif
	])
])
GP_CONFIG_MSG([ttylock locking], [${ttylock_msg}])

lockdev_msg=no
AS_IF([$try_lockdev], [dnl
	AC_CHECK_LIB([lockdev], [dev_lock], [dnl
		AC_CHECK_HEADER([lockdev.h], [dnl
			AC_DEFINE([HAVE_LOCKDEV], [1],
			          [Define if you have dev_lock/lockdev based locking.])
			lockdev_msg=yes
			SERIAL_LIBS=-llockdev
		])
	])
])
GP_CONFIG_MSG([lockdev locking], [${lockdev_msg}])

])dnl $have_serial

AC_SUBST([SERIAL_LIBS])


dnl ---------------------------------------------------------------------------
dnl libusb
dnl ---------------------------------------------------------------------------

dnl libpthread is dynamically linked by libusb, which is dynamically loaded
dnl but newer Linux systems need it on startup, not later. So link libgphoto2_port
dnl against libpthread
AC_CHECK_LIB([pthread], [pthread_create])

dnl ---------------------------------------------------------------------------
dnl libusb1
dnl ---------------------------------------------------------------------------
GP_CONFIG_MSG([USB ports])
GP_CHECK_LIBRARY([LIBUSB1], [libusb-1.0], [>= 1.0.0],
                 [libusb.h], [libusb_init], [dnl
        case "$host" in
          *-linux*)
	  IOLIB_LIST="$IOLIB_LIST usb1 usbdiskdirect usbscsi"
          ;;
          *)
	  IOLIB_LIST="$IOLIB_LIST usb1"
          ;;
        esac

	# only in libusbx? but not in traditional libusb 1.0
	# note that the library is not always named libusb-1.0.so , e.g. on freebsd it is not.
	save_LIBS="$LIBS"
	LIBS="$LIBS $LIBUSB1_LIBS"
	AC_CHECK_FUNC([libusb_strerror], [dnl
		AC_DEFINE([HAVE_LIBUSB_STRERROR], [1],
		          [Define if libusb-1.0 has libusb_strerror])
	])
	AC_CHECK_FUNC([libusb_wrap_sys_device], [dnl
		AC_DEFINE([HAVE_LIBUSB_WRAP_SYS_DEVICE], [1],
		          [Define if libusb-1.0 has libusb_wrap_sys_device])
	])

	save_CFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS $LIBUSB1_CFLAGS"

	AC_MSG_CHECKING([whether we have LIBUSB_OPTION_NO_DEVICE_DISCOVERY])
	AC_COMPILE_IFELSE([dnl
	AC_LANG_PROGRAM([[
	# include "libusb.h"
	]], [[
	enum libusb_option opt = LIBUSB_OPTION_NO_DEVICE_DISCOVERY;
	]])dnl
	], [dnl
	  AC_DEFINE([HAVE_LIBUSB_OPTION_NO_DEVICE_DISCOVERY], [1],
		    [whether we have LIBUSB_OPTION_NO_DEVICE_DISCOVERY])
	  AC_MSG_RESULT([yes])
	], [dnl
	  AC_MSG_RESULT([no])
	])

	LIBS="$save_LIBS"
	CFLAGS="$save_CFLAGS"
], [],
		[default-on],
		[http://libusb.sourceforge.net/]
)

# We select the old libusb 0 only if we are not doing libusb1
GP_CHECK_LIBRARY([LIBUSB], [libusb], [>= 0.1.5],
                 [usb.h], [usb_open], [dnl
	case "$host" in
	  *-*-mingw* | *-*-cygwin* | *-*-msvc* )
	  dnl Windows itself has usb.h we mis-detect, in that case remove access to the other usb.h
	  IOLIB_LIST="$IOLIB_LIST usb"
	  ;;
          *-linux*)
	  AS_VAR_IF([have_LIBUSB1], [yes], [dnl
		GP_CONFIG_MSG([libusb0 support],
		              [detected, but disabled (libusb1 support is enabled)])
          ], [dnl
		IOLIB_LIST="$IOLIB_LIST usb usbdiskdirect usbscsi"
          ])
	  ;;
	  *)
	  AS_VAR_IF([have_LIBUSB1], [yes], [dnl
		GP_CONFIG_MSG([libusb0 support],
		              [detected, but disabled (libusb1 support is enabled)])
          ], [dnl
		IOLIB_LIST="$IOLIB_LIST usb"
          ])
	  ;;
	esac
], [],
		[default-on],
		[http://libusb.sourceforge.net/]
)
dnl depends on libusb's availability
GP_UDEV([test "x$have_LIBUSB" = xyes || test "x$have_LIBUSB1" = xyes])


AC_ARG_ENABLE([disk],
	[AS_HELP_STRING([--disable-disk],
	                [disable the 'disk' port driver])],
	[],
	[enable_disk=yes])

dnl disk port also works everywhere, but it's not really necessary
AS_VAR_IF([enable_disk], [yes], [dnl
	IOLIB_LIST="$IOLIB_LIST disk"
])

AC_ARG_ENABLE([vusb],
	[AS_HELP_STRING([--enable-vusb],
	                [enable the 'vusb' virtual USB port driver])],
	[], [dnl
		enable_vusb=no
		GP_CONFIG_MSG([virtual USB support],
		              [disabled, no virtual PTP test camera])
	]
)

dnl disk port also works everywhere, but it's not really necessary
AS_VAR_IF([enable_vusb], [yes], [dnl
	IOLIB_LIST="$IOLIB_LIST vusb"
])

AC_ARG_ENABLE([ptpip],
	[AS_HELP_STRING([--disable-ptpip],
	                [disable the 'ptpip' port driver for TCP/IP connected PTP cameras])],
	[], [dnl
		enable_ptpip=yes
])
dnl ptpip - works 'everywhere' with TCP/IP stack.
AS_VAR_IF([enable_ptpip], [yes], [dnl
	IOLIB_LIST="$IOLIB_LIST ptpip"
])

# ----------------------------------------------------------------------
# Define IOLIB stuff
# ----------------------------------------------------------------------


dnl ---------------------------------------------------------------------------
dnl iolib installation directory
dnl ---------------------------------------------------------------------------

GP_DRIVERDIR([iolibdir], [IOLIBS], [iolibs (port libraries)])


dnl ---------------------------------------------------------------------------
dnl List of iolibs (port drivers)
dnl ---------------------------------------------------------------------------
AC_SUBST([IOLIB_LIST])
for x in ${IOLIB_LIST}; do
    IOLIB_LTLIST="${IOLIB_LTLIST} ${x}.la"
done
AC_SUBST([IOLIB_LTLIST])

sorted_iolib_list="$(echo "${IOLIB_LIST}" | tr ' ' '\n' | sort | ${SED} '/^$/d' | tr '\n' ' ' | ${SED} 's/ $//')"
AC_DEFINE_UNQUOTED([IOLIB_LIST], ["${sorted_iolib_list}"],
                   [Define as string containing a list of the iolibs])
GP_CONFIG_MSG([General])
GP_CONFIG_MSG([IOLIBs], [${sorted_iolib_list}])


dnl --------------------------------------------------------------------
dnl guess directory to install *.pc into
dnl --------------------------------------------------------------------
pkgconfigdir='${libdir}/pkgconfig'
AC_SUBST([pkgconfigdir])


dnl ----------------------------------------------------------------------
dnl GNU regex?
dnl ----------------------------------------------------------------------
AC_CHECK_FUNC([re_compile_pattern],
              [AC_DEFINE([HAVE_GNU_REGEX], [1],
	                 [Define if you have GNU regex.])])


dnl ----------------------------------------------------------------------
dnl disable debugging if requested
dnl ----------------------------------------------------------------------
AC_ARG_ENABLE([debug],
  [AS_HELP_STRING([--disable-debug],
                  [do not include debugging code])],
  [dnl
    AS_VAR_IF([enableval], [no], [dnl
      AC_DEFINE([DISABLE_DEBUGGING], [1],
                [Define if you want to disabled debugging.])
    ])
])


# ----------------------------------------------------------------------
# gtk-doc?
# ----------------------------------------------------------------------
GP_CONFIG_MSG([Documentation])
GP_BUILD_GTK_DOCS()dnl


dnl ---------------------------------------------------------------------------
dnl Turn on all warnings when using gcc
dnl ---------------------------------------------------------------------------
AS_VAR_IF([GCC], [yes], [dnl
	CFLAGS="$CFLAGS -Wall"
	CFLAGS="$CFLAGS -Wmissing-declarations -Wmissing-prototypes"
])

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


dnl ---------------------------------------------------------------------------
dnl Make it easier to possibly move the libraries around
dnl ---------------------------------------------------------------------------

AC_SUBST([libgphoto2_port_la],
         ['${top_builddir}/libgphoto2_port/libgphoto2_port.la'])


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
  tests/Makefile
  doc/Makefile
])
AC_OUTPUT

GP_CONFIG_OUTPUT

# End of file.
