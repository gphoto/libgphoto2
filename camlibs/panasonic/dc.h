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

#ifdef sun
	typedef uint8_t u_int8_t;
	typedef uint32_t u_int32_t;
#endif

#define DSC1	1	/* DC1000 */
#define DSC2	2	/* DC1580 */

typedef enum {
	unavailable	= -1,
	normal		= 0,
	fine		= 1,
	superfine	= 2
} dsc_quality_t;
	
typedef struct {
	gpio_device		*dev;
	gpio_device_settings 	settings;
	CameraFilesystem	*fs;
	char			*buf;
	int			size;	
} dsc_t;

#define DSC_BLOCKSIZE	  0x400   /* amount of image data transfered in a single packet */
#define DSC_BUFSIZE	  0x406   /* largest possible amount of data transfered in a single packet */
#define DSC_MAXIMAGESIZE  0xfffff /* largest possible file that can be uploaded */
#define DSC_FILENAMEFMT	  "dsc%04i.jpg" /* format of image file names */
#define DSC_THUMBNAMEFMT  "dsc%04i-thumbnail.jpg" /* format of thumbnail file names */

#define DSC_PAUSE	4 /* seconds to wait for camera to redraw screen */

#define DSC_FULLIMAGE	0 
#define DSC_THUMBNAIL	1

#define EDSCSERRNO     -1 /* see errno value */
#define EDSCUNKNWN	0 /* unknown error code */
#define EDSCBPSRNG	1 /* bps out of range */
#define EDSCBADNUM	2 /* bad image number */
#define EDSCBADRSP	3 /* bad response */
#define EDSCBADDSC	4 /* bad camera model */
#define EDSCOVERFL	5 /* overfolw */
#define EDSCMAXERR	5 /* highest used error code */

static const char
	c_prefix[13] = /* generic command prefix */
	{ 'M', 'K', 'E', ' ', 'D', 'S', 'C', ' ', 'P', 'C', ' ', ' ' },

	r_prefix[13] = /* generic response prefix */
	{ 'M', 'K', 'E', ' ', 'P', 'C', ' ', ' ', 'D', 'S', 'C', ' ' };

int dsc1_sendcmd(dsc_t *dsc, u_int8_t cmd, void *data, int size);
	/* Send command with 'data' of 'size' to DSC */

int dsc1_retrcmd(dsc_t *dsc);
	/* Retrieve command and its data from DSC */

int dsc1_setbaudrate(dsc_t *dsc, int speed);
	/* Set baud rate of connection. Part of hand shake procedure 	*/
	/* Returns GP_OK if succesful and GP_ERROR otherwise.		*/
	
int dsc1_getmodel(dsc_t *dsc);
	/* Returns camera (sub)model, DSC1: DC1000, DSC2: DC1580, 	*/
	/* 0: unknown, or GP_ERROR. Part of hand shake procedure. 	*/

void dsc_dumpmem(void *buf, int size);
	/* Print size bytes of memory at the buf pointer.		*/
		
const char *dsc_strerror(int error);
	/* Convert error numbers into readable messages. 		*/

char *dsc_msgprintf(char *format, ...);
	/* Format message string. 					*/

void dsc_debugprint(char *file, char *message);
	/* Print debug information, including file name, to stderr.	*/
	
void dsc_errorprint(int error, char *file, char *function, int line);
	/* Print information on error, including file name, function 	*/
	/* name and line number.					*/

void dsc_print_status(Camera *camera, char *format, ...);
	/* Call gp_camera_status() and dsc_debugprint() with the same 	*/
	/* message.							*/

void dsc_print_message(Camera *camera, char *format, ...);
	/* Call gp_camera_message() and dsc_debugprint() with the same 	*/
	/* message.							*/

/******************************************************************************/

/* Pre-procesor macros for verbose messaging and debugging */

#define DEBUG_PRINT(ARGS) \
	dsc_debugprint(__FILE__, dsc_msgprintf ARGS ); 

#define RETURN_ERROR(ERROR, FUNCTION) { \
	dsc_errorprint(ERROR, __FILE__, #FUNCTION, __LINE__); \
	return GP_ERROR; \
	}

/* End of dc.h */
