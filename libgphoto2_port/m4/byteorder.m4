dnl AC_NEED_BYTEORDER_H ( HEADER-TO-GENERATE )
dnl $Id$
dnl Copyright 2001 by Dan Fandrich <dan@coneharvesters.com>
dnl This file may be copied and used freely without restrictions.  No warranty
dnl is expressed or implied.
dnl
dnl Create a header file that guarantees that byte swapping macros of the
dnl ntohl variety as well as the extended types included in OpenBSD and
dnl NetBSD such as le32toh are defined.  If possible, the standard ntohl
dnl are overloaded as they are optimized for the given platform, but when
dnl this is not possible (e.g. on a big-endian machine) they are defined
dnl in this file.

dnl Look for a symbol in a header file
dnl AC_HAVE_SYMBOL ( IDENTIFIER, HEADER-FILE, ACTION-IF-FOUND, ACTION-IF-NOT-FOUND )
AC_DEFUN(AC_HAVE_SYMBOL,
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
AC_DEFUN(AC_NEED_BYTEORDER_H,
[
cat > "$1" << EOF
/* This file is generated automatically by configure */
/* It is valid only for the system type ${host} */

#ifndef __BYTEORDER_H
#define __BYTEORDER_H

EOF

dnl First, do an endian check
AC_C_BIGENDIAN

dnl Look for NetBSD-style extended byte swapping macros
AC_HAVE_SYMBOL(le32toh,sys/endian.h,
 [HAVE_LE32TOH=1
 cat >> "$1" << EOF
/* extended byte swapping macros are already available */
#include <sys/endian.h>

EOF],

[

dnl Look for standard byte swapping macros
AC_HAVE_SYMBOL(ntohl,arpa/inet.h,
 [cat >> "$1" << EOF
/* ntohl and relatives live here */
#include <arpa/inet.h>

EOF],

 [AC_HAVE_SYMBOL(ntohl,netinet/in.h,
  [cat >> "$1" << EOF
/* ntohl and relatives live here */
#include <netinet/in.h>

EOF],true)])
])

dnl Look for generic byte swapping macros

dnl OpenBSD
AC_HAVE_SYMBOL(swap32,sys/endian.h,
 [cat >> "$1" << EOF
/* swap32 and swap16 are defined in sys/endian.h */
EOF],

 [
dnl Linux GLIBC
  AC_HAVE_SYMBOL(bswap_32,byteswap.h,
   [cat >> "$1" << EOF
/* Define generic byte swapping functions */
#include <byteswap.h>
#define swap16(x) bswap_16(x)
#define swap32(x) bswap_32(x)

EOF],

   [
dnl FreeBSD
  	AC_HAVE_SYMBOL(__byte_swap_long,sys/types.h,
  	 [cat >> "$1" << EOF
/* Define generic byte swapping functions */
#include <sys/types.h>
#define swap16(x) __byte_swap_word(x)
#define swap32(x) __byte_swap_long(x)

EOF],

  	 [
  	dnl OS X
  	AC_HAVE_SYMBOL(NXSwapLong,machine/byte_order.h,
  	 [cat >> "$1" << EOF
/* Define generic byte swapping functions */
#include <machine/byte_order.h>
#define swap16(x) NXSwapShort(x)
#define swap32(x) NXSwapLong(x)

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

EOF
else
 cat >> "$1" << EOF
/* Use these as generic byteswapping macros on this little endian system */
#define swap16(x)		ntohs(x)
#define swap32(x)		ntohl(x)

EOF
fi
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
#define htobe16(x) htons(x)
#define htobe32(x) htonl(x)
#define be16toh(x) ntohs(x)
#define be32toh(x) ntohl(x)

#define HTOBE16(x) (x) = htobe16(x)
#define HTOBE32(x) (x) = htobe32(x)
#define BE32TOH(x) (x) = be32toh(x)
#define BE16TOH(x) (x) = be16toh(x)

EOF

 if test $ac_cv_c_bigendian = yes; then
  cat >> "$1" << EOF
/* Define our own extended byte swapping macros for big-endian machines */
#define htole16(x)      swap16(x)
#define htole32(x)      swap32(x)
#define le16toh(x)      swap16(x)
#define le32toh(x)      swap32(x)

#define HTOLE16(x)      (x) = htole16(x)
#define HTOLE32(x)      (x) = htole32(x)
#define LE16TOH(x)      (x) = le16toh(x)
#define LE32TOH(x)      (x) = le32toh(x)

EOF
 else
  cat >> "$1" << EOF
/* On little endian machines, these macros are null */
#define htole16(x)      (x)
#define htole32(x)      (x)
#define htole64(x)      (x)
#define le16toh(x)      (x)
#define le32toh(x)      (x)
#define le64toh(x)      (x)

#define HTOLE16(x)      (void) (x)
#define HTOLE32(x)      (void) (x)
#define HTOLE64(x)      (void) (x)
#define LE16TOH(x)      (void) (x)
#define LE32TOH(x)      (void) (x)
#define LE64TOH(x)      (void) (x)

EOF
 fi
fi

cat >> "$1" << EOF
/* Define the C99 standard length-specific integer types */
#include <_stdint.h>

EOF

case "${host_cpu}" in
 i[3456]86)
  cat >> "$1" << EOF
/* Here are some macros to create integers from a byte array */
/* These are used to get and put integers from/into a uint8_t array */
/* with a specific endianness.  This is the most portable way to generate */
/* and read messages to a network or serial device.  Each member of a */
/* packet structure must be handled separately. */

/* The i386 and compatibles can handle unaligned memory access, */
/* so use the optimized macros above to do this job */
#define be16atoh(x)     be16toh(*(uint16_t*)(x))
#define be32atoh(x)     be32toh(*(uint32_t*)(x))
#define le16atoh(x)     le16toh(*(uint16_t*)(x))
#define le32atoh(x)     le32toh(*(uint32_t*)(x))

#define htobe16a(a,x)   *(uint16_t*)(a) = htobe16(x)
#define htobe32a(a,x)   *(uint32_t*)(a) = htobe32(x)
#define htole16a(a,x)   *(uint16_t*)(a) = htole16(x)
#define htole32a(a,x)   *(uint32_t*)(a) = htole32(x)

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
#define le16atoh(x)     ((uint16_t)(((x)[1]<<8)|(x)[0]))
#define le32atoh(x)     ((uint32_t)(((x)[3]<<24)|((x)[2]<<16)|((x)[1]<<8)|(x)[0]))

#define htobe16a(a,x)   (a)[0]=(uint8_t)((x)>>8), (a)[1]=(x)&0xff
#define htobe32a(a,x)   (a)[0]=(uint8_t)((x)>>24), (a)[1]=(uint8_t)((x)>>16), (a)[2]=(uint8_t)((x)>>8), (a)[3]=(x)&0xff
#define htole16a(a,x)   (a)[1]=(uint8_t)((x)>>8), (a)[0]=(x)&0xff
#define htole32a(a,x)   (a)[3]=(uint8_t)((x)>>24), (a)[2]=(uint8_t)((x)>>16), (a)[1]=(uint8_t)((x)>>8), (a)[0]=(x)&0xff

EOF
  ;;
esac
]

cat >> "$1" << EOF
#endif /*__BYTEORDER_H*/
EOF])
