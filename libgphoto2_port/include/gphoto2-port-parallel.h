#ifndef _GP_PORT_PARALLEL_H_
#define _GP_PORT_PARALLEL_H_

/* PARALLEL port prefix for enumeration */

/* Linux */
#ifdef __linux
#define GP_PORT_PARALLEL_PREFIX 		"/dev/lp%i"
#define	GP_PORT_PARALLEL_RANGE_LOW		0
#define	GP_PORT_PARALLEL_RANGE_HIGH	16
#endif

/* BSD */
#if defined(__FreeBSD__) || defined(__NetBSD__)
#define GP_PORT_PARALLEL_PREFIX 		NULL
#define	GP_PORT_PARALLEL_RANGE_LOW		0
#define	GP_PORT_PARALLEL_RANGE_HIGH	0
#endif

/* Solaris */
#ifdef sun
#  ifdef i386 
#    define GP_PORT_PARALLEL_PREFIX "/dev/lp%i" /* x86 parallel port prefix*/
#    define GP_PORT_PARALLEL_RANGE_LOW	1
#    define GP_PORT_PARALLEL_RANGE_HIGH	16
#  else
#    define GP_PORT_PARALLEL_PREFIX "/dev/bpp%02i" /* Sparc parallel port prefix*/
#    define GP_PORT_PARALLEL_RANGE_LOW	0
#    define GP_PORT_PARALLEL_RANGE_HIGH	16
#  endif
#endif


/* BeOS */
#ifdef beos
/* ????????????? */
#define GP_PORT_PARALLEL_PREFIX		NULL
#define	GP_PORT_PARALLEL_RANGE_LOW		0
#define	GP_PORT_PARALLEL_RANGE_HIGH	0
#endif

/* Windows */
#ifdef WIN
#define GP_PORT_PARALLEL_PREFIX 		"LPT%i:"
#define	GP_PORT_PARALLEL_RANGE_LOW		0
#define	GP_PORT_PARALLEL_RANGE_HIGH	16
#endif

/* OS/2 */
#ifdef OS2
#define GP_PORT_PARALLEL_PREFIX "LPT%i"
#define GP_PORT_PARALLEL_RANGE_LOW         1
#define GP_PORT_PARALLEL_RANGE_HIGH        4
#endif

/* Others? */

/* Default */
#ifndef GP_PORT_PARALLEL_PREFIX
#warning GP_PORT_PARALLEL_PREFIX not defined. Enumeration will fail
#define GP_PORT_PARALLEL_PREFIX 		NULL
#define	GP_PORT_PARALLEL_RANGE_LOW		0
#define	GP_PORT_PARALLEL_RANGE_HIGH	0
#endif

/* PARALLEL port specific settings */
typedef struct {
        char port[128];
} gp_port_parallel_settings;

extern struct gp_port_operations gp_port_parallel_operations;

#endif /* _GP_PORT_PARALLEL_H_ */


