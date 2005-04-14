/* tty device */
/* linux        "/dev/cua1" */
/* NEWS-OS 4.2  "/dev/tty00" */
/* NEWS-OS 6.X  "/dev/term/00" */
/* EWS4800      "/dev/term/00" */
/* SunOS 4.1.4  "/dev/ttya" */
/* Solaris2.3   "/dev/ttya" */
/* BSD/OS 2.0   "/dev/tty01" */
/* FreeBSD      "/dev/cuaa0" */
/* Windows NT "COM2" */
/* NEXTSTEP "/dev/ttya" */
/* IRIX  "/dev/ttyf1" */

#include <config.h>

#if defined(WIN32) || defined(OS2)
#define RSPORT "COM1"
#else
#ifdef DOS
#define RSPORT "AUX1"
#else
#ifdef __NetBSD__
#define RSPORT  "/dev/tty00"
#else
#define RSPORT  "/dev/ttyS0"
#endif
#endif
#endif


#define BOFRAME 0xC0
#define EOFRAME 0xC1
#define CESCAPE 0x7D

#define DEFAULTBAUD B38400


#define JPEG 0
#define JPEG_T 1
#define PMP 2
#define PMX 3

/* for function prototypes */
#ifdef STDC_HEADERS
# define P__(x) x
#else
# error STDC_HEADERS NOT defined
# error Compilation of functions defined with P__ will fail anyway, so
# error it does not hurt if we abort it here a little earlier and more
# error to the point.
# define P__(x) ()
#endif

void    Exit P__((int));

#define RETRY   100	

#define THUMBNAIL_WIDTH 52
#define THUMBNAIL_HEIGHT 36
#define PICTURE_WIDTH 640
#define PICTURE_HEIGHT 480

#define THUMBNAIL_MAXSIZ 4*1024
#define JPEG_MAXSIZ     130*1024
#define PMF_MAXSIZ 3*1024

