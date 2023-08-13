/* This file is generated automatically by configure */
/* It is valid only for the system type x86_64-pc-linux-gnu */

#ifndef GP_BYTEORDER_H
#define GP_BYTEORDER_H

/* ntohl and relatives live here */
#include <arpa/inet.h>
#define __HAVE_NTOHL

/* Define generic byte swapping functions */
#include <byteswap.h>
#define swap16(x) bswap_16(x)
#define swap32(x) bswap_32(x)
#define swap64(x) bswap_64(x)

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

/* Define the C99 standard length-specific integer types */
#include <stdint.h>

/* Here are some macros to create integers from a byte array */
/* These are used to get and put integers from/into a uint8_t array */
/* with a specific endianness.  This is the most portable way to generate */
/* and read messages to a network or serial device.  Each member of a */
/* packet structure must be handled separately. */

/* Non-optimized but portable macros */
#define be16atoh(x)     ((uint16_t)(((x)[0]<<8)|(x)[1]))
#define be32atoh(x)     ((uint32_t)(((x)[0]<<24)|((x)[1]<<16)|((x)[2]<<8)|(x)[3]))
#define be64atoh_x(x,off,shift) 	(((uint64_t)((x)[off]))<<shift)
#define be64atoh(x)     ((uint64_t)(be64atoh_x(x,0,56)|be64atoh_x(x,1,48)|be64atoh_x(x,2,40)| \
        be64atoh_x(x,3,32)|be64atoh_x(x,4,24)|be64atoh_x(x,5,16)|be64atoh_x(x,6,8)|((x)[7])))
#define le16atoh(x)     ((uint16_t)(((x)[1]<<8)|(x)[0]))
#define le32atoh(x)     ((uint32_t)(((x)[3]<<24)|((x)[2]<<16)|((x)[1]<<8)|(x)[0]))
#define le64atoh_x(x,off,shift) (((uint64_t)(x)[off])<<shift)
#define le64atoh(x)     ((uint64_t)(le64atoh_x(x,7,56)|le64atoh_x(x,6,48)|le64atoh_x(x,5,40)| \
        le64atoh_x(x,4,32)|le64atoh_x(x,3,24)|le64atoh_x(x,2,16)|le64atoh_x(x,1,8)|((x)[0])))

#define htobe16a(a,x)   (a)[0]=(uint8_t)((x)>>8), (a)[1]=(uint8_t)(x)
#define htobe32a(a,x)   (a)[0]=(uint8_t)((x)>>24), (a)[1]=(uint8_t)((x)>>16), \
        (a)[2]=(uint8_t)((x)>>8), (a)[3]=(uint8_t)(x)
#define htobe64a(a,x)   (a)[0]=(uint8_t)((x)>>56), (a)[1]=(uint8_t)((x)>>48), \
        (a)[2]=(uint8_t)((x)>>40), (a)[3]=(uint8_t)((x)>>32), \
        (a)[4]=(uint8_t)((x)>>24), (a)[5]=(uint8_t)((x)>>16), \
        (a)[6]=(uint8_t)((x)>>8), (a)[7]=(uint8_t)(x)
#define htole16a(a,x)   (a)[1]=(uint8_t)((x)>>8), (a)[0]=(uint8_t)(x)
#define htole32a(a,x)   (a)[3]=(uint8_t)((x)>>24), (a)[2]=(uint8_t)((x)>>16), \
        (a)[1]=(uint8_t)((x)>>8), (a)[0]=(uint8_t)(x)
#define htole64a(a,x)   (a)[7]=(uint8_t)((x)>>56), (a)[6]=(uint8_t)((x)>>48), \
        (a)[5]=(uint8_t)((x)>>40), (a)[4]=(uint8_t)((x)>>32), \
        (a)[3]=(uint8_t)((x)>>24), (a)[2]=(uint8_t)((x)>>16), \
        (a)[1]=(uint8_t)((x)>>8), (a)[0]=(uint8_t)(x)

#endif /* GP_BYTEORDER_H */
