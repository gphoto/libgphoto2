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
	
	This is a Panasonic DC1580 camera gPhoto library source code.
*/	

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <memory.h>
#include "dc.h"

#ifndef __FILE__
#  define __FILE__ "dc.c"
#endif

dsc_t	*dsc = NULL;
int	glob_debug = 0;

int dsc_setbaudrate(dsc_t *dsc, int speed) {
	
	u_int8_t	s_bps;
	int		s;

	static const char
		command[6] = /* set baud rate */
			{ 0x00, 0x00, 0x00, 0x01, 0x04 },
		response[5] = /* response ok */
			{ 0x00, 0x00, 0x00, 0x01 };
	
	switch (speed) {
		
		case 9600:
			s_bps = 0x02;
			break;

		case 19200:
			s_bps = 0x0d;
			break;

		case 38400:
			s_bps = 0x01;
			break;

		case 57600:
			s_bps = 0x03;
			break;

		case 115200:
			s_bps = 0x00;
			break;
			
		default:
			DBUG_RETURN(EDSCBPSRNG, dsc_setbaudrate, GP_ERROR);
	}

	gpio_write(dsc->dev, (char *)c_prefix, 12);
	gpio_write(dsc->dev, (char *)command, 5);
	gpio_write(dsc->dev, &s_bps, 1);

	s = gpio_read(dsc->dev, dsc->buf, 18);
	if (glob_debug)
		dsc_dumpmem(dsc->buf, s);
	if (s == 18) {
		if (memcmp(dsc->buf, r_prefix, 12) != 0 ||
				memcmp(dsc->buf + 12, response, 4) != 0) 
			s = 0;
	}
	else
		DBUG_RETURN(EDSCNOANSW, dsc_setbaudrate, GP_ERROR);
		/* no answer from camera */
	
	sleep(DSC_PAUSE/2);

	dsc->settings.serial.speed = speed;
	gpio_set_settings(dsc->dev, dsc->settings);

	DBUG_PRINT_1("Baudrate set to: %d", speed);
	
	return GP_OK;
}

int dsc_getmodel(dsc_t *dsc) {
	
	int	s;

	static const char
		command[6] = /* get camera model */
			{ 0x00, 0x00, 0x00, 0x00, 0x02 },
		response[9] = 
			{ 0x00, 0x00, 0x00, 0x04, 0x03, 'D', 'S', 'C' };
	
	gpio_write(dsc->dev, (char *)c_prefix, 12);
	gpio_write(dsc->dev, (char *)command, 5);

	s = gpio_read(dsc->dev, dsc->buf, 21);
	if (glob_debug)
		dsc_dumpmem(dsc->buf, s);
	if (s != 21 &&
			memcmp(dsc->buf, r_prefix, 12) != 0 &&
			memcmp(dsc->buf + 12, response, 8) != 0)
		DBUG_RETURN(EDSCNOANSW, dsc_getmodel, GP_ERROR);
		/* no answer from camera */

	DBUG_PRINT_1("Camera model is: %c", dsc->buf[20]);
		
	switch (dsc->buf[20]) {
		case '1':
			return DSC1;

		case '2':
			return DSC2;

		default:
			return 0;
	}
	return GP_ERROR;
}

int dsc_handshake(dsc_t *dsc, int speed) {
	
	if (dsc_setbaudrate(dsc, speed) != GP_OK)
		return GP_ERROR;
	
	if (dsc_getmodel(dsc) != DSC2)
		DBUG_RETURN(EDSCBADDSC, dsc_handshake, GP_ERROR);
		/* bad camera model */
	
   	DBUG_PRINT("Handshake: OK.");

	return GP_OK;	
}

int dsc_dumpmem(void *buf, int size) {
	
	int	i;
	
	fprintf(stderr, "\ndc.c: dsc_dumpmem(): size: %i, contents:\n", size);
	for (i = 0; i < size; i ++)
		fprintf(
			stderr,
			*((char*)buf + i) >= 32 &&
			*((char*)buf + i) < 127 ? "%c" : "\\x%02x",
			(u_int8_t)*((char*)buf + i)
		);
	fprintf(stderr, "\n\n");
	
	return GP_OK;
}
	
const char *dsc_strerror(dsc_error lasterror) {

	static const char * const errorlist[] = {
			"Unknown error code!",
			"Baud rate out of range",
			"No answer from camera",
			"Read time out",
			"Could not reset camera",
			"Bad image number",
			"Bad protocol",
			"Bad response",
			"Bad camera model"
	};

	return	lasterror.lerror == EDSCSERRNO ?
		strerror(lasterror.lerrno) :
		lasterror.lerror < 1 || lasterror.lerror > EDSCMAXERR ?
		errorlist[0] :
		errorlist[lasterror.lerror];
}

/* End of dc.c */
