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
	
	This is a Panasonic DC series common routines.
*/	

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <memory.h>
#include "dc.h"
#include "dc1000.h"

#ifndef __FILE__
#  define __FILE__ "dc.c"
#endif

int dsc1_sendcmd(dsc_t *dsc, u_int8_t cmd, void *data, int size) {

	int	i;

	DEBUG_PRINT(("Sending command: 0x%02x, data size: %i.", cmd, size));

	memset(dsc->buf, 0, DSC_BUFSIZE);
	
	memcpy(dsc->buf, c_prefix, 12);

	for (i = 0; i < 4; i++) 
		dsc->buf[DSC1_BUF_SIZE + i] = (u_int8_t)(size >> 8*(3 - i));

	dsc->buf[DSC1_BUF_CMD] = cmd;
	
	if (DSC_BUFSIZE - DSC1_BUF_DATA <= size) {
		RETURN_ERROR(EDSCOVERFL, dsc1_sendcmd)
		/* overflow */
	}
	
	if (data && 0 < size)
		memcpy(&dsc->buf[DSC1_BUF_DATA], data, size);
	
	return gpio_write(dsc->dev, dsc->buf, 17 + size);
}

int dsc1_retrcmd(dsc_t *dsc) {
	
	int	result = GP_ERROR; 
	int	s;
	
	if ((s = gpio_read(dsc->dev, dsc->buf, 17)) == GP_ERROR)
		return GP_ERROR;
	
	if (dsc->debug && 0 < s)
		dsc_dumpmem(dsc->buf, s);
	
	if (s != 17 || 	memcmp(dsc->buf, r_prefix, 12) != 0 ) 
		RETURN_ERROR(EDSCBADRSP, dsc1_retrcmd)
		/* bad response */
	else
		result = dsc->buf[DSC1_BUF_CMD];
	
	dsc->size = 
		(u_int32_t)dsc->buf[DSC1_BUF_SIZE + 3] |
		((u_int8_t)dsc->buf[DSC1_BUF_SIZE + 2] << 8) |						
		((u_int8_t)dsc->buf[DSC1_BUF_SIZE + 1] << 16) |
		((u_int8_t)dsc->buf[DSC1_BUF_SIZE] << 24);		
	
	if (DSC_BUFSIZE < dsc->size) {
		RETURN_ERROR(EDSCOVERFL, dsc1_retrcmd); 
		/* overflow */
	}
		
	if (gpio_read(dsc->dev, dsc->buf, dsc->size) != dsc->size)
		return GP_ERROR;

	DEBUG_PRINT(("Retrieved command: %i.", result));
	
	return result;
}

int dsc1_setbaudrate(dsc_t *dsc, int speed) {
	
	u_int8_t	s_bps;
	int		s;

	DEBUG_PRINT(("Setting baud rate to: %i.", speed));
	
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
			RETURN_ERROR(EDSCBPSRNG, dsc1_setbaudrate);
	}

	if (dsc1_sendcmd(dsc, DSC1_CMD_SET_BAUD, &s_bps, 1) != GP_OK)
		return GP_ERROR;
	
	if (dsc1_retrcmd(dsc) != DSC1_RSP_OK) 
		RETURN_ERROR(EDSCBADRSP, dsc1_setbaudrate);
		/* bad response */	

	sleep(DSC_PAUSE/2);

	dsc->settings.serial.speed = speed;
	if (gpio_set_settings(dsc->dev, dsc->settings) != GP_OK)
		return GP_ERROR;

	DEBUG_PRINT(("Baudrate set to: %i.", speed));
	
	return GP_OK;
}

int dsc1_getmodel(dsc_t *dsc) {
	
	int	s;

	static const char	response[3] = { 'D', 'S', 'C' };
	
	DEBUG_PRINT(("Getting camera model."));
	
	if (dsc1_sendcmd(dsc, DSC1_CMD_GET_MODEL, NULL, 0) != GP_OK)
		return GP_ERROR;
	
	if (dsc1_retrcmd(dsc) != DSC1_RSP_MODEL || 
			memcmp(dsc->buf, response, 3) != 0)
		RETURN_ERROR(EDSCBADRSP, dsc1_getmodel);
		/* bad response */	

	DEBUG_PRINT(("Camera model is: %c.", dsc->buf[3]));
		
	switch (dsc->buf[3]) {
		case '1':
			return DSC1;

		case '2':
			return DSC2;

		default:
			return 0;
	}
	return GP_ERROR;
}

void dsc_dumpmem(void *buf, int size) {
	
	int	i;
	
	fprintf(stderr, "\nMemory dump: size: %i, contents:\n", size);
	for (i = 0; i < size; i ++)
		fprintf(
			stderr,
			*((char*)buf + i) >= 32 &&
			*((char*)buf + i) < 127 ? "%c" : "\\x%02x",
			(u_int8_t)*((char*)buf + i)
		);
	fprintf(stderr, "\n\n");
}
	
const char *dsc_strerror(int error) {

	static const char * const errorlist[] = {
			"Unknown error code",
			"Baud rate out of range",
			"Bad image number",
			"Bad response",
			"Bad camera model",
			"Overflow"
	};
			
	if (error == EDSCSERRNO)
		return strerror(errno);

	if (error < 1 || EDSCMAXERR < error)
		return errorlist[EDSCUNKNWN];
	
	return errorlist[error];
}

char *dsc_msgprintf(char *format, ...) {

	va_list		pvar;
	static char	msg[512];
	
	va_start(pvar, format);
	vsprintf(msg, format, pvar);
	va_end(pvar);
	return msg;
}

void dsc_debugprint(char *file, char *message) {

	fprintf(stderr, "%s: %s\n", file, message);
}

void dsc_errorprint(int error, char *file, char *function, int line) {

	dsc_debugprint(file, 
		dsc_msgprintf("%s(): return from line %u, code: %i, errno: %i, %s\n",
			function, line, error, errno, dsc_strerror(error)));
}

void dsc_print_status(Camera *camera, char *format, ...) {
	
	va_list		pvar;
	char		str[80];
	
	va_start(pvar, format);
	vsprintf(str, format, pvar);	
	va_end(pvar);
	
	if (camera->debug)
		dsc_debugprint(__FILE__, str);
	gp_camera_status(camera, str);
}

void dsc_print_message(Camera *camera, char *format, ...) {
	
	va_list		pvar;
	char		str[512];
	
	va_start(pvar, format);
	vsprintf(str, format, pvar);	
	va_end(pvar);
	
	if (camera->debug)
		dsc_debugprint(__FILE__, str);
	gp_camera_message(camera, str);
}

/* End of dc.c */
