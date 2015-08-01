dnl AC_NEED_BYTEORDER_H ( HEADER-TO-GENERATE )
dnl Originally written by Dan Fandrich <dan@coneharvesters.com>
dnl My contribution is hereby placed into the public domain.
dnl No warranty is expressed or implied.
dnl
dnl Create a header file that guarantees that byte swapping macros of the
dnl ntohl variety as well as the extended types included in OpenBSD and
dnl NetBSD such as le32toh are defined.  If possible, the standard ntohl
dnl are overloaded as they are optimized for the given platform, but when
dnl this is not possible (e.g. on a big-endian machine) they are defined
dnl in this file.

dnl Look for a symbol in a header file
dnl AC_HAVE_SYMBOL ( IDENTIFIER, HEADER-FILE, ACTION-IF-FOUND, ACTION-IF-NOT-FOUND )
AC_DEFUN([AC_HAVE_SYMBOL],
[
AC_MSG_CHECKING(for $1 in $2)
AC_EGREP_CPP([symbol is present|\<$1\>],[
#include <$2>
#ifdef $1
 	symbol is present
#endif
	], 
[AC_MSG_RESULT(yes)
$3
],
[AC_MSG_RESULT(no)
$4
])])


dnl Create a header file that defines extended byte swapping macros
AC_DEFUN([AC_NEED_BYTEORDER_H],
[
ac_dir=`AS_DIRNAME(["$1"])`
if test "$ac_dir" != "$1" && test "$ac_dir" != .; then
  # The file is in a subdirectory.
  test ! -d "$ac_dir" && AS_MKDIR_P(["$ac_dir"])
fi

# We're only interested in the target CPU, but it's not always set
effective_target="$target"
if test "x$effective_target" = xNONE -o "x$effective_target" = x ; then
	effective_target="$host"
fi
AC_SUBST(effective_target)

cat > "$1" << EOF
/* This file is generated automatically by configure */
/* It is valid only for the system type ${effective_target} */

#ifndef __BYTEORDER_H
#define __BYTEORDER_H

EOF

dnl First, do an endian check
AC_C_BIGENDIAN

dnl Look for NetBSD-style extended byte swapping macros
AC_HAVE_SYMBOL(le32toh,machine/endian.h,
 [HAVE_LE32TOH=1
 cat >> "$1" << EOF
/* extended byte swapping macros are already available */
#include <machine/endian.h>

EOF],

[

dnl Look for standard byte swapping macros
AC_HAVE_SYMBOL(ntohl,arpa/inet.h,
 [cat >> "$1" << EOF
/* ntohl and relatives live here */
#include <arpa/inet.h>
#define __HAVE_NTOHL

EOF],

 [AC_HAVE_SYMBOL(ntohl,netinet/in.h,
  [cat >> "$1" << EOF
/* ntohl and relatives live here */
#include <netinet/in.h>
#define __HAVE_NTOHL

EOF],true)])
])

dnl Look for generic byte swapping macros

dnl OpenBSD
AC_HAVE_SYMBOL(swap32,machine/endian.h,
 [cat >> "$1" << EOF
/* swap32 and swap16 are defined in machine/endian.h */

EOF],

 [
dnl Linux GLIBC
  AC_HAVE_SYMBOL(bswap_32,byteswap.h,
   [cat >> "$1" << EOF
/* Define generic byte swapping functions */
#include <byteswap.h>
#define swap16(x) bswap_16(x)
#define swap32(x) bswap_32(x)
#define swap64(x) bswap_64(x)

EOF],

   [
dnl NetBSD
  	AC_HAVE_SYMBOL(bswap32,machine/endian.h,
    dnl We're already including machine/endian.h if this test succeeds
  	 [cat >> "$1" << EOF
/* Define generic byte swapping functions */
EOF
	if test "$HAVE_LE32TOH" != "1"; then
		echo '#include <machine/endian.h>'>> "$1"
	fi
cat >> "$1" << EOF
#define swap16(x) bswap16(x)
#define swap32(x) bswap32(x)
#define swap64(x) bswap64(x)

EOF],

   [
dnl FreeBSD
  	AC_HAVE_SYMBOL(__byte_swap_long,sys/types.h,
  	 [cat >> "$1" << EOF
/* Define generic byte swapping functions */
#include <sys/types.h>
#define swap16(x) __byte_swap_word(x)
#define swap32(x) __byte_swap_long(x)
/* No optimized 64 bit byte swapping macro is available */
#define swap64(x) ((uint64_t)(((uint64_t)(x) << 56) & 0xff00000000000000ULL | \\
			      ((uint64_t)(x) << 40) & 0x00ff000000000000ULL | \\
			      ((uint64_t)(x) << 24) & 0x0000ff0000000000ULL | \\
			      ((uint64_t)(x) << 8)  & 0x000000ff00000000ULL | \\
			      ((x) >> 8)  & 0x00000000ff000000ULL | \\
			      ((x) >> 24) & 0x0000000000ff0000ULL | \\
			      ((x) >> 40) & 0x000000000000ff00ULL | \\
			      ((x) >> 56) & 0x00000000000000ffULL))

EOF],

  	 [
dnl OS X
  	AC_HAVE_SYMBOL(NXSwapLong,machine/byte_order.h,
  	 [cat >> "$1" << EOF
/* Define generic byte swapping functions */
#include <machine/byte_order.h>
#define swap16(x) NXSwapShort(x)
#define swap32(x) NXSwapLong(x)
#define swap64(x) NXSwapLongLong(x)

EOF],
         [
	if test $ac_cv_c_bigendian = yes; then
		cat >> "$1" << EOF
/* No other byte swapping functions are available on this big-endian system */
#define swap16(x)	((uint16_t)(((x) << 8) | ((uint16_t)(x) >> 8)))
#define swap32(x)	((uint32_t)(((uint32_t)(x) << 24) & 0xff000000UL | \\
				    ((uint32_t)(x) << 8)  & 0x00ff0000UL | \\
				    ((x) >> 8)  & 0x0000ff00UL | \\
				    ((x) >> 24) & 0x000000ffUL))
#define swap64(x) ((uint64_t)(((uint64_t)(x) << 56) & 0xff00000000000000ULL | \\
			      ((uint64_t)(x) << 40) & 0x00ff000000000000ULL | \\
			      ((uint64_t)(x) << 24) & 0x0000ff0000000000ULL | \\
			      ((uint64_t)(x) << 8)  & 0x000000ff00000000ULL | \\
			      ((x) >> 8)  & 0x00000000ff000000ULL | \\
			      ((x) >> 24) & 0x0000000000ff0000ULL | \\
			      ((x) >> 40) & 0x000000000000ff00ULL | \\
			      ((x) >> 56) & 0x00000000000000ffULL))

EOF
	else
 cat >> "$1" << EOF
/* Use these as generic byteswapping macros on this little endian system */
/* on windows we might not have ntohs / ntohl without including winsock.dll,
 * so use generic macros */
#ifdef __HAVE_NTOHL
# define swap16(x)	htons(x)
# define swap32(x)	htonl(x)
#else
# define swap16(x)	((uint16_t)(((x) << 8) | ((uint16_t)(x) >> 8)))
# define swap32(x)	((uint32_t)((((uint32_t)(x) << 24) & 0xff000000UL) | \\
				    (((uint32_t)(x) << 8)  & 0x00ff0000UL) | \\
				    (((x) >> 8)  & 0x0000ff00UL) | \\
				    (((x) >> 24) & 0x000000ffUL)))
#endif
/* No optimized 64 bit byte swapping macro is available */
#define swap64(x) ((uint64_t)((((uint64_t)(x) << 56) & 0xff00000000000000ULL) | \\
			      (((uint64_t)(x) << 40) & 0x00ff000000000000ULL) | \\
			      (((uint64_t)(x) << 24) & 0x0000ff0000000000ULL) | \\
			      (((uint64_t)(x) << 8)  & 0x000000ff00000000ULL) | \\
			      (((x) >> 8)  & 0x00000000ff000000ULL) | \\
			      (((x) >> 24) & 0x0000000000ff0000ULL) | \\
			      (((x) >> 40) & 0x000000000000ff00ULL) | \\
			      (((x) >> 56) & 0x00000000000000ffULL)))

EOF
	fi
])
  	  ])
    ])
  ])
])


[
if test "$HAVE_LE32TOH" != "1"; then
 cat >> "$1" << EOF
/* The byte swapping macros have the form: */
/*   EENN[a]toh or htoEENN[a] where EE is be (big endian) or */
/* le (little-endian), NN is 16 or 32 (number of bits) and a, */
/* if present, indicates that the endian side is a pointer to an */
/* array of uint8_t bytes instead of an integer of the specified length. */
/* h refers to the host's ordering method. */

/* So, to convert a 32-bit integer stored in a buffer in little-endian */
/* format into a uint32_t usable on this machine, you could use: */
/*   uint32_t value = le32atoh(&buf[3]); */
/* To put that value back into the buffer, you could use: */
/*   htole32a(&buf[3], value); */

/* Define aliases for the standard byte swapping macros */
/* Arguments to these macros must be properly aligned on natural word */
/* boundaries in order to work properly on all architectures */
#ifndef htobe16
# ifdef __HAVE_NTOHL
#  define htobe16(x) htons(x)
# else
#  ifdef WORDS_BIGENDIAN
#   define htobe16(x) (x)
#  else
#   define htobe16(x) swap16(x)
#  endif
# endif
#endif
#ifndef htobe32
# ifdef __HAVE_NTOHL
#  define htobe32(x) htonl(x)
# else
#  ifdef WORDS_BIGENDIAN
#   define htobe32(x) (x)
#  else
#   define htobe32(x) swap32(x)
#  endif
# endif
#endif
#ifndef be16toh
# define be16toh(x) htobe16(x)
#endif
#ifndef be32toh
# define be32toh(x) htobe32(x)
#endif

#define HTOBE16(x) (x) = htobe16(x)
#define HTOBE32(x) (x) = htobe32(x)
#define BE32TOH(x) (x) = be32toh(x)
#define BE16TOH(x) (x) = be16toh(x)

EOF

 if test $ac_cv_c_bigendian = yes; then
  cat >> "$1" << EOF
/* Define our own extended byte swapping macros for big-endian machines */
#ifndef htole16
# define htole16(x)      swap16(x)
#endif
#ifndef htole32
# define htole32(x)      swap32(x)
#endif
#ifndef le16toh
# define le16toh(x)      swap16(x)
#endif
#ifndef le32toh
# define le32toh(x)      swap32(x)
#endif
#ifndef le64toh
# define le64toh(x)      swap64(x)
#endif

#ifndef htobe64
# define htobe64(x)      (x)
#endif
#ifndef be64toh
# define be64toh(x)      (x)
#endif

#define HTOLE16(x)      (x) = htole16(x)
#define HTOLE32(x)      (x) = htole32(x)
#define LE16TOH(x)      (x) = le16toh(x)
#define LE32TOH(x)      (x) = le32toh(x)
#define LE64TOH(x)      (x) = le64toh(x)

#define HTOBE64(x)      (void) (x)
#define BE64TOH(x)      (void) (x)

EOF
 else
  cat >> "$1" << EOF
/* On little endian machines, these macros are null */
#ifndef htole16
# define htole16(x)      (x)
#endif
#ifndef htole32
# define htole32(x)      (x)
#endif
#ifndef htole64
# define htole64(x)      (x)
#endif
#ifndef le16toh
# define le16toh(x)      (x)
#endif
#ifndef le32toh
# define le32toh(x)      (x)
#endif
#ifndef le64toh
# define le64toh(x)      (x)
#endif

#define HTOLE16(x)      (void) (x)
#define HTOLE32(x)      (void) (x)
#define HTOLE64(x)      (void) (x)
#define LE16TOH(x)      (void) (x)
#define LE32TOH(x)      (void) (x)
#define LE64TOH(x)      (void) (x)

/* These don't have standard aliases */
#ifndef htobe64
# define htobe64(x)      swap64(x)
#endif
#ifndef be64toh
# define be64toh(x)      swap64(x)
#endif

#define HTOBE64(x)      (x) = htobe64(x)
#define BE64TOH(x)      (x) = be64toh(x)

EOF
 fi
fi

cat >> "$1" << EOF
/* Define the C99 standard length-specific integer types */
#include <_stdint.h>

EOF

case "${effective_target}" in
 i[3456]86-*)
  cat >> "$1" << EOF
/* Here are some macros to create integers from a byte array */
/* These are used to get and put integers from/into a uint8_t array */
/* with a specific endianness.  This is the most portable way to generate */
/* and read messages to a network or serial device.  Each member of a */
/* packet structure must be handled separately. */

/* The i386 and compatibles can handle unaligned memory access, */
/* so use the optimized macros above to do this job */
#ifndef be16atoh
# define be16atoh(x)     be16toh(*(uint16_t*)(x))
#endif
#ifndef be32atoh
# define be32atoh(x)     be32toh(*(uint32_t*)(x))
#endif
#ifndef be64atoh
# define be64atoh(x)     be64toh(*(uint64_t*)(x))
#endif
#ifndef le16atoh
# define le16atoh(x)     le16toh(*(uint16_t*)(x))
#endif
#ifndef le32atoh
# define le32atoh(x)     le32toh(*(uint32_t*)(x))
#endif
#ifndef le64atoh
# define le64atoh(x)     le64toh(*(uint64_t*)(x))
#endif

#ifndef htob16a
# define htobe16a(a,x)   *(uint16_t*)(a) = htobe16(x)
#endif
#ifndef htobe32a
# define htobe32a(a,x)   *(uint32_t*)(a) = htobe32(x)
#endif
#ifndef htobe64a
# define htobe64a(a,x)   *(uint64_t*)(a) = htobe64(x)
#endif
#ifndef htole16a
# define htole16a(a,x)   *(uint16_t*)(a) = htole16(x)
#endif
#ifndef htole32a
# define htole32a(a,x)   *(uint32_t*)(a) = htole32(x)
#endif
#ifndef htole64a
# define htole64a(a,x)   *(uint64_t*)(a) = htole64(x)
#endif

EOF
  ;;

 *)
  cat >> "$1" << EOF
/* Here are some macros to create integers from a byte array */
/* These are used to get and put integers from/into a uint8_t array */
/* with a specific endianness.  This is the most portable way to generate */
/* and read messages to a network or serial device.  Each member of a */
/* packet structure must be handled separately. */

/* Non-optimized but portable macros */
#define be16atoh(x)     ((uint16_t)(((x)[0]<<8)|(x)[1]))
#define be32atoh(x)     ((uint32_t)(((x)[0]<<24)|((x)[1]<<16)|((x)[2]<<8)|(x)[3]))
#define be64atoh_x(x,off,shift) 	(((uint64_t)((x)[off]))<<shift)
#define be64atoh(x)     ((uint64_t)(be64atoh_x(x,0,56)|be64atoh_x(x,1,48)|be64atoh_x(x,2,40)| \\
        be64atoh_x(x,3,32)|be64atoh_x(x,4,24)|be64atoh_x(x,5,16)|be64atoh_x(x,6,8)|((x)[7])))
#define le16atoh(x)     ((uint16_t)(((x)[1]<<8)|(x)[0]))
#define le32atoh(x)     ((uint32_t)(((x)[3]<<24)|((x)[2]<<16)|((x)[1]<<8)|(x)[0]))
#define le64atoh_x(x,off,shift) (((uint64_t)(x)[off])<<shift)
#define le64atoh(x)     ((uint64_t)(le64atoh_x(x,7,56)|le64atoh_x(x,6,48)|le64atoh_x(x,5,40)| \\
        le64atoh_x(x,4,32)|le64atoh_x(x,3,24)|le64atoh_x(x,2,16)|le64atoh_x(x,1,8)|((x)[0])))

#define htobe16a(a,x)   (a)[0]=(uint8_t)((x)>>8), (a)[1]=(uint8_t)(x)
#define htobe32a(a,x)   (a)[0]=(uint8_t)((x)>>24), (a)[1]=(uint8_t)((x)>>16), \\
        (a)[2]=(uint8_t)((x)>>8), (a)[3]=(uint8_t)(x)
#define htobe64a(a,x)   (a)[0]=(uint8_t)((x)>>56), (a)[1]=(uint8_t)((x)>>48), \\
        (a)[2]=(uint8_t)((x)>>40), (a)[3]=(uint8_t)((x)>>32), \\
        (a)[4]=(uint8_t)((x)>>24), (a)[5]=(uint8_t)((x)>>16), \\
        (a)[6]=(uint8_t)((x)>>8), (a)[7]=(uint8_t)(x)
#define htole16a(a,x)   (a)[1]=(uint8_t)((x)>>8), (a)[0]=(uint8_t)(x)
#define htole32a(a,x)   (a)[3]=(uint8_t)((x)>>24), (a)[2]=(uint8_t)((x)>>16), \\
        (a)[1]=(uint8_t)((x)>>8), (a)[0]=(uint8_t)(x)
#define htole64a(a,x)   (a)[7]=(uint8_t)((x)>>56), (a)[6]=(uint8_t)((x)>>48), \\
        (a)[5]=(uint8_t)((x)>>40), (a)[4]=(uint8_t)((x)>>32), \\
        (a)[3]=(uint8_t)((x)>>24), (a)[2]=(uint8_t)((x)>>16), \\
        (a)[1]=(uint8_t)((x)>>8), (a)[0]=(uint8_t)(x)

EOF
  ;;
esac
]

cat >> "$1" << EOF
#endif /*__BYTEORDER_H*/
EOF])
