/*
	$Id$

	Copyright (c) 2000 Mariusz Zynel <mariusz@mizar.org> (gPhoto port)
	Copyright (C) 2000 Fredrik Roubert <roubert@df.lth.se> (idea)
	Copyright (C) 1999 Galen Brooks <galen@nine.com> (DC1580 code)

	This file is part of the gPhoto project and may only be used,  modified,
	and distributed under the terms of the gPhoto project license,  COPYING.
	By continuing to use, modify, or distribute  this file you indicate that
	you have read the license, understand and accept it fully.
	
	THIS  SOFTWARE IS PROVIDED AS IS AND COME WITH NO WARRANTY  OF ANY KIND,
	EITHER  EXPRESSED OR IMPLIED.  IN NO EVENT WILL THE COPYRIGHT  HOLDER BE
	LIABLE FOR ANY DAMAGES RESULTING FROM THE USE OF THIS SOFTWARE.

	Note:
	
	This header contains common  typedefs,  constants and utility  functions
	for Panasonic DC1000 and DC1580 cameras.
*/	

#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <gphoto2.h>
#include <gpio/gpio.h>

#ifdef sun
	typedef uint8_t u_int8_t;
	typedef uint32_t u_int32_t;
#endif

#define DSC1	1	/* DC1000 */
#define DSC2	2	/* DC1580 */

typedef enum {
	unknown		= 0,
	dsc1		= DSC1,
	dsc2		= DSC2
} dsc_protocol_t;

typedef enum {
	unavailable	= -1,
	normal		= 0,
	fine		= 1,
	superfine	= 2
} dsc_quality_t;
	
typedef struct {
	int		lerror, lerrno;
} dsc_error;	

typedef struct {
	gpio_device		*dev;
	gpio_device_settings 	settings;
	dsc_protocol_t		type;
	dsc_error		lasterror;
	char 			model[64];	
	char			*buf;
} dsc_t;

#define DSC_BLOCKSIZE	0x400 /* amount of data transfered in a single packet */
#define DSC_MAXIMAGE	0xff  /* highest possible number of an image */

#ifndef DSC_PAUSE
#  define DSC_PAUSE	5 /* seconds to wait for camera to redraw screen */
#endif

#define DSC_READTIMEOUT	2 /* seconds to wait for data from terminal */

#define DSC_FULLIMAGE	0 
#define DSC_THUMBNAIL	1

#define EDSCSERRNO     -1 /* see errno value */
#define EDSCUNKNWN	0 /* unknown error code */
#define EDSCBPSRNG	1 /* bps out of range */
#define EDSCNOANSW	2 /* no answer from camera */
#define EDSCRTMOUT	3 /* read time out */
#define EDSCNRESET	4 /* could not reset camera */
#define EDSCBADNUM	5 /* bad image number */
#define EDSCBADPCL	6 /* bad protocol */
#define EDSCBADRSP	7 /* bad response */
#define EDSCBADDSC	8 /* bad camera model */
#define EDSCMAXERR	8 /* highest used error code */

extern dsc_t	*dsc; 
extern int	glob_debug; /* 0 - debug mode off, 1 - debug mode on */

static const char
	c_prefix[13] = /* generic command prefix */
	{ 'M', 'K', 'E', ' ', 'D', 'S', 'C', ' ', 'P', 'C', ' ', ' ' },

	r_prefix[13] = /* generic response prefix */
	{ 'M', 'K', 'E', ' ', 'P', 'C', ' ', ' ', 'D', 'S', 'C', ' ' };

int dsc_setbaudrate(dsc_t *dsc, int speed);
	/* Set baud rate of connection. Part of hand shake procedure 	*/
	/* Returns GP_OK if succesful and GP_ERROR otherwise.		*/
	
int dsc_getmodel(dsc_t *dsc);
	/* Returns camera (sub)model, DSC1: DC1000, DSC2: DC1580, 	*/
	/* 0: unknown, or GP_ERROR. Part of hand shake procedure. 	*/

int dsc_handshake(dsc_t *dsc, int speed);
	/* Negotiate transmition speed, check camera model. 		*/
	/* Returns GP_OK if succesful and GP_ERROR otherwise.		*/	

int dsc_dumpmem(void *buf, int size);
	/* Print size bytes of memory at the buf pointer.		*/
		
const char *dsc_strerror(dsc_error lasterror);
	/* Convert error numbers into readable messages. 		*/

/******************************************************************************/

/* Pre-procesor macros for verbose messaging and debugging */

#define DBUG_PRINT(FORMAT) \
	if (glob_debug) \
		fprintf( \
			stderr, \
			__FILE__ ": " FORMAT "\n" \
		)
		
#define DBUG_PRINT_1(FORMAT, ARG1) \
	if (glob_debug) \
		fprintf( \
			stderr, \
			__FILE__ ": " FORMAT "\n", ARG1 \
		)

#define DBUG_PRINT_2(FORMAT, ARG1, ARG2) \
	if (glob_debug) \
		fprintf( \
			stderr, \
			__FILE__ ": " FORMAT "\n", ARG1, ARG2 \
		)

#define DBUG_PRINT_3(FORMAT, ARG1, ARG2, ARG3) \
	if (glob_debug) \
		fprintf( \
			stderr, \
			__FILE__ ": " FORMAT "\n", ARG1, ARG2, ARG3 \
		)
		
#define DBUG_PRINT_ERROR(DSCERROR, FUNCTION) \
	if (glob_debug) \
		fprintf( \
			stderr, \
			__FILE__ ": " #FUNCTION \
			"() return from line %u, code: %u, errno: %u, %s\n", \
			__LINE__, DSCERROR.lerror, DSCERROR.lerrno, dsc_strerror(DSCERROR) \
		)

#define DBUG_RETURN(ERROR, FUNCTION, RESULT) { \
	dsc->lasterror.lerror = ERROR; \
	dsc->lasterror.lerrno = errno; \
	DBUG_PRINT_ERROR(dsc->lasterror, FUNCTION); \
	return RESULT; }

/* End of dc.h */
