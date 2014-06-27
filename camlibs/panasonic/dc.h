/*
	$Id$

	Copyright 2000 Mariusz Zynel <mariusz@mizar.org> (gPhoto port)
	Copyright 2000 Fredrik Roubert <roubert@df.lth.se> (idea)
	Copyright 1999 Galen Brooks <galen@nine.com> (DC1580 code)

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

#include <stdio.h>
#include <errno.h>
#include <_stdint.h>
#include <gphoto2/gphoto2.h>

#define DSC1	1	/* DC1000 */
#define DSC2	2	/* DC1580 */

typedef enum {
	unavailable	= -1,
	normal		= 0,
	fine		= 1,
	superfine	= 2
} dsc_quality_t;
	
struct _CameraPrivateLibrary {
	char			*buf;
	int			size;
};

#define DSC_BLOCKSIZE	  0x400   /* amount of image data transferred in a single packet */
#define DSC_BUFSIZE	  0x406   /* largest possible amount of data transferred in a single packet */
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

int dsc1_sendcmd(Camera *camera, uint8_t cmd, void *data, int size);
	/* Send command with 'data' of 'size' to DSC */

int dsc1_retrcmd(Camera *camera);
	/* Retrieve command and its data from DSC */

int dsc1_setbaudrate(Camera *camera, int speed);
	/* Set baud rate of connection. Part of hand shake procedure 	*/
	/* Returns GP_OK if succesful and GP_ERROR otherwise.		*/
	
int dsc1_getmodel(Camera *camera);
	/* Returns camera (sub)model, DSC1: DC1000, DSC2: DC1580, 	*/
	/* 0: unknown, or GP_ERROR. Part of hand shake procedure. 	*/

void dsc_dumpmem(void *buf, int size);
	/* Print size bytes of memory at the buf pointer.		*/
		
const char *dsc_strerror(int error);
	/* Convert error numbers into readable messages. 		*/

char *dsc_msgprintf(char *format, ...);
	/* Format message string. 					*/

void dsc_errorprint(int error, char *file, int line);
	/* Print information on error, including file name, function 	*/
	/* name and line number.					*/

/******************************************************************************/

/* Pre-procesor macros for verbose messaging and debugging */

#define DEBUG_PRINT_LOW(ARGS) \
        GP_DEBUG( "%s: %s", __FILE__, dsc_msgprintf ARGS );

#define DEBUG_PRINT_MEDIUM(ARGS) \
        GP_DEBUG( "%s: %s", __FILE__, dsc_msgprintf ARGS );

#define DEBUG_PRINT_HIGH(ARGS) \
        GP_DEBUG( "%s: %s", __FILE__, dsc_msgprintf ARGS );

#define RETURN_ERROR(ERROR) { \
        dsc_errorprint(ERROR, __FILE__, __LINE__); \
        return GP_ERROR; \
        }
        
#define CHECK(OPERATION) \
        if ((result = OPERATION) < 0) { \
                dsc_errorprint(EDSCSERRNO, __FILE__, __LINE__); \
                return result; \
        }

/* End of dc.h */
