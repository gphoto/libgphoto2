/* config.h.in.  Generated from configure.ac by autoheader.  */

/* text domain for string translations */
#define GETTEXT_PACKAGE_LIBGPHOTO2 "gphoto2"
#define GETTEXT_PACKAGE_LIBGPHOTO2_PORT "gphoto2_port"

/* The actually defined set of camlibs to build */
#define GP_CAMLIB_SET "canon"

/* define when the camlib set to buidl is non-standard */
#cmakedefine GP_CAMLIB_SET_IS_NONSTANDARD

/* If defined, the camlibs which are skipped due to missing dependencies */
#cmakedefine GP_CAMLIB_SET_SKIPPING

/* Define if there is asm .symver support. */
#cmakedefine HAVE_ASM_SYMVERS

/* The C compiler we are using */
#cmakedefine HAVE_CC

/* Define to 1 if you have the Mac OS X function CFLocaleCopyCurrent in the
   CoreFoundation framework. */
#cmakedefine HAVE_CFLOCALECOPYCURRENT

/* Define to 1 if you have the Mac OS X function CFPreferencesCopyAppValue in
   the CoreFoundation framework. */
#cmakedefine HAVE_CFPREFERENCESCOPYAPPVALUE

/* Define if the GNU dcgettext() function is already present or preinstalled.
   */
#cmakedefine HAVE_DCGETTEXT

/* Define to 1 if you have the <dirent.h> header file, and it defines `DIR'.
   */
#cmakedefine HAVE_DIRENT_H

/* Define to 1 if you have the <dlfcn.h> header file. */
#cmakedefine HAVE_DLFCN_H

/* Define to 1 if you have the `getenv' function. */
#cmakedefine HAVE_GETENV

/* Define to 1 if you have the `getopt' function. */
#cmakedefine HAVE_GETOPT

/* Define to 1 if you have the <getopt.h> header file. */
#cmakedefine HAVE_GETOPT_H

/* Define to 1 if you have the `getopt_long' function. */
#cmakedefine HAVE_GETOPT_LONG

/* Define if the GNU gettext() function is already present or preinstalled. */
#cmakedefine HAVE_GETTEXT

/* Define to 1 if you have the `gmtime_r' function. */
#cmakedefine HAVE_GMTIME_R

/* Define if you have the iconv() function and it works. */
#cmakedefine HAVE_ICONV

/* Define to 1 if you have the `inet_aton' function. */
#cmakedefine HAVE_INET_ATON

/* Define to 1 if you have the <inttypes.h> header file. */
#cmakedefine HAVE_INTTYPES_H

/* Define to 1 if you have the <langinfo.h> header file. */
#cmakedefine HAVE_LANGINFO_H

/* whether we compile with libcurl support */
#cmakedefine HAVE_LIBCURL

/* whether we compile with libexif support */
#cmakedefine HAVE_LIBEXIF

/* whether we use a version of libexif with ExifData.ifd[[]] */
#cmakedefine HAVE_LIBEXIF_IFD

/* whether we compile with gdlib support */
#cmakedefine HAVE_LIBGD

/* Define to 1 if you have the `ibs' library (-libs). */
#cmakedefine HAVE_LIBIBS

/* define if building with libjpeg */
#cmakedefine HAVE_LIBJPEG

/* Define to 1 if you have the `m' library (-lm). */
#cmakedefine HAVE_LIBM

/* define if we found LIBWS232 and its headers */
#cmakedefine HAVE_LIBWS232

/* whether we compile with libxml-2.0 support */
#cmakedefine HAVE_LIBXML2

/* Define to 1 if you have the <limits.h> header file. */
#define HAVE_LIMITS_H

/* Define to 1 if you have the <locale.h> header file. */
#cmakedefine HAVE_LOCALE_H

/* Define to 1 if you have the `localtime_r' function. */
#cmakedefine HAVE_LOCALTIME_R

/* Define to 1 if you have the `lstat' function. */
#cmakedefine HAVE_LSTAT

/* Define to 1 if you have the <mcheck.h> header file. */
#cmakedefine HAVE_MCHECK_H

/* Define to 1 if you have the <memory.h> header file. */
#cmakedefine HAVE_MEMORY_H

/* Define to 1 if you have the `mkdir' function. */
#cmakedefine HAVE_MKDIR

/* Define to 1 if you have the <ndir.h> header file, and it defines `DIR'. */
#cmakedefine HAVE_NDIR_H

/* Define to 1 if you have the `rand_r' function. */
#cmakedefine HAVE_RAND_R

/* Define to 1 if you have the `setenv' function. */
#cmakedefine HAVE_SETENV

/* Define to 1 if you have the `snprintf' function. */
#cmakedefine HAVE_SNPRINTF

/* Define to 1 if you have the `sprintf' function. */
#cmakedefine HAVE_SPRINTF

/* Define to 1 if you have the `statvfs' function. */
#cmakedefine HAVE_STATVFS

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H

/* Define to 1 if you have the `strcpy' function. */
#define HAVE_STRCPY

/* Define to 1 if you have the `strdup' function. */
#define HAVE_STRDUP

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H

/* Define to 1 if you have the `strncpy' function. */
#define HAVE_STRNCPY

/* Define to 1 if `f_blocks' is a member of `struct statvfs'. */
#cmakedefine HAVE_STRUCT_STATVFS_F_BLOCKS

/* Define to 1 if you have the <sys/dir.h> header file, and it defines `DIR'.
   */
#cmakedefine HAVE_SYS_DIR_H

/* Define to 1 if you have the <sys/mman.h> header file. */
#cmakedefine HAVE_SYS_MMAN_H

/* Define to 1 if you have the <sys/mount.h> header file. */
#cmakedefine HAVE_SYS_MOUNT_H

/* Define to 1 if you have the <sys/ndir.h> header file, and it defines `DIR'.
   */
#cmakedefine HAVE_SYS_NDIR_H

/* Define to 1 if you have the <sys/param.h> header file. */
#cmakedefine HAVE_SYS_PARAM_H

/* Define to 1 if you have the <sys/select.h> header file. */
#cmakedefine HAVE_SYS_SELECT_H

/* Define to 1 if you have the <sys/statvfs.h> header file. */
#cmakedefine HAVE_SYS_STATVFS_H

/* Define to 1 if you have the <sys/stat.h> header file. */
#cmakedefine HAVE_SYS_STAT_H

/* Define to 1 if you have the <sys/time.h> header file. */
#cmakedefine HAVE_SYS_TIME_H

/* Define to 1 if you have the <sys/types.h> header file. */
#cmakedefine HAVE_SYS_TYPES_H

/* Define to 1 if you have the <sys/user.h> header file. */
#cmakedefine HAVE_SYS_USER_H

/* Define to 1 if you have the <sys/vfs.h> header file. */
#cmakedefine HAVE_SYS_VFS_H

/* whether struct tm has tm_gmtoff field */
#cmakedefine HAVE_TM_GMTOFF

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H

/* Whether we have the va_copy() function */
#define HAVE_VA_COPY 1

/* Define to 1 if you have the `vsnprintf' function. */
#define HAVE_VSNPRINTF 1

/* Define as const if the declaration of iconv() needs const. */
#cmakedefine ICONV_CONST

/* Define to the sub-directory where libtool stores uninstalled libraries. */
#cmakedefine LT_OBJDIR

/* gphoto development mailing list */
#define MAIL_GPHOTO_DEVEL "<gphoto-devel@lists.sourceforge.net>"

/* gphoto translation mailing list */
#cmakedefine MAIL_GPHOTO_TRANSLATION

/* gphoto user mailing list */
#cmakedefine MAIL_GPHOTO_USER

/* Name of package */
#cmakedefine PACKAGE

/* Define to the address where bug reports for this package should be sent. */
#cmakedefine PACKAGE_BUGREPORT

/* Define to the full name of this package. */
#define PACKAGE_NAME "gphoto"

/* Define to the full name and version of this package. */
#cmakedefine PACKAGE_STRING

/* Define to the one symbol short name of this package. */
#cmakedefine PACKAGE_TARNAME

/* Define to the home page for this package. */
#cmakedefine PACKAGE_URL

/* Define to the version of this package. */
#define PACKAGE_VERSION "0"

/* Define to 1 if you have the ANSI C header files. */
#cmakedefine STDC_HEADERS

/* Define to 1 if your <sys/time.h> declares `struct tm'. */
#cmakedefine TM_IN_SYS_TIME

/* camera list with support status */
#cmakedefine URL_DIGICAM_LIST

/* gphoto project home page */
#cmakedefine URL_GPHOTO_HOME

/* gphoto github project page */
#cmakedefine URL_GPHOTO_PROJECT

/* jphoto home page */
#cmakedefine URL_JPHOTO_HOME

/* information about using USB mass storage */
#cmakedefine URL_USB_MASSSTORAGE

/* Version number of package */
#cmakedefine VERSION

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
# if defined __BIG_ENDIAN__
#  define WORDS_BIGENDIAN 1
# endif
#else
# ifndef WORDS_BIGENDIAN
#  undef WORDS_BIGENDIAN
# endif
#endif

/* Enable large inode numbers on Mac OS X 10.5.  */
#ifndef _DARWIN_USE_64_BIT_INODE
# define _DARWIN_USE_64_BIT_INODE 1
#endif

/* Number of bits in a file offset, on hosts where this is settable. */
#cmakedefine _FILE_OFFSET_BITS

/* Define for large files, on AIX-style hosts. */
#cmakedefine _LARGE_FILES

/* Define to empty if `const' does not conform to ANSI C. */
#cmakedefine const

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
#cmakedefine inline
#endif

/* Define to `unsigned int' if <sys/types.h> does not define. */
#cmakedefine size_t

/* __va_copy() was the originally proposed name */
#cmakedefine va_copy

#define IOLIB_LIST "usb1"

#define URL_USB_MASSSTORAGE "http://www.linux-usb.org/USB-guide/x498.html"

#cmakedefine GPHOTO2_FLAT_LAYOUT 
#cmakedefine GPHOTO2_FLAT_LAYOUT_IO_PREFIX "@GPHOTO2_FLAT_LAYOUT_IO_PREFIX@"
#cmakedefine GPHOTO2_FLAT_LAYOUT_CAM_PREFIX "@GPHOTO2_FLAT_LAYOUT_CAM_PREFIX@"

#cmakedefine HAVE_LIBUSB_WRAP_SYS_DEVICE
