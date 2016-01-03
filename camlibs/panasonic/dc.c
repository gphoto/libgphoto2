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

        This is a Panasonic DC series common routines.
*/
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <string.h>
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include "dc.h"
#include "dc1000.h"

#ifndef __FILE__
#  define __FILE__ "dc.c"
#endif

#define GP_MODULE "dc"


static const char
	c_prefix[13] = /* generic command prefix */
	{ 'M', 'K', 'E', ' ', 'D', 'S', 'C', ' ', 'P', 'C', ' ', ' ' },

	r_prefix[13] = /* generic response prefix */
	{ 'M', 'K', 'E', ' ', 'P', 'C', ' ', ' ', 'D', 'S', 'C', ' ' };


int dsc1_sendcmd(Camera *camera, uint8_t cmd, void *data, int size) {

        int     i;

        DEBUG_PRINT_MEDIUM(("Sending command: 0x%02x, data size: %i.", cmd, size));

        memset(camera->pl->buf, 0, DSC_BUFSIZE);

        memcpy(camera->pl->buf, c_prefix, 12);

        for (i = 0; i < 4; i++)
                camera->pl->buf[DSC1_BUF_SIZE + i] = (uint8_t)(size >> 8*(3 - i));

        camera->pl->buf[DSC1_BUF_CMD] = cmd;

        if (DSC_BUFSIZE - DSC1_BUF_DATA <= size) {
                RETURN_ERROR(EDSCOVERFL)
                /* overflow */
        }

        if (data && 0 < size)
                memcpy(&camera->pl->buf[DSC1_BUF_DATA], data, size);

        return gp_port_write(camera->port, camera->pl->buf, 17 + size);
}

int dsc1_retrcmd(Camera *camera) {

        int     result = GP_ERROR;
        int     s;

        if ((s = gp_port_read(camera->port, camera->pl->buf, 17)) == GP_ERROR)
                return GP_ERROR;

/*      Make sense in debug only. Done on gp_port level.
        if (0 < s)
                dsc_dumpmem(camera->pl->buf, s);
*/
                
        if (s != 17 ||  memcmp(camera->pl->buf, r_prefix, 12) != 0 )
                RETURN_ERROR(EDSCBADRSP)
                /* bad response */
        else
                result = camera->pl->buf[DSC1_BUF_CMD];

        camera->pl->size =
                (uint32_t)camera->pl->buf[DSC1_BUF_SIZE + 3] |
                ((uint8_t)camera->pl->buf[DSC1_BUF_SIZE + 2] << 8) |
                ((uint8_t)camera->pl->buf[DSC1_BUF_SIZE + 1] << 16) |
                ((uint8_t)camera->pl->buf[DSC1_BUF_SIZE] << 24);

        if (DSC_BUFSIZE < camera->pl->size) {
                RETURN_ERROR(EDSCOVERFL);
                /* overflow */
        }

        if (gp_port_read(camera->port, camera->pl->buf, camera->pl->size) != camera->pl->size)
                return GP_ERROR;

        DEBUG_PRINT_MEDIUM(("Retrieved command: %i.", result));

        return result;
}

int dsc1_setbaudrate(Camera *camera, int speed) {

	GPPortSettings settings;
        uint8_t        s_bps;
        int             result;

        DEBUG_PRINT_MEDIUM(("Setting baud rate to: %i.", speed));

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
                        RETURN_ERROR(EDSCBPSRNG);
        }

        if (dsc1_sendcmd(camera, DSC1_CMD_SET_BAUD, &s_bps, 1) != GP_OK)
                return GP_ERROR;

        if (dsc1_retrcmd(camera) != DSC1_RSP_OK)
                RETURN_ERROR(EDSCBADRSP);
                /* bad response */

        sleep(DSC_PAUSE/2);

	CHECK (gp_port_get_settings(camera->port, &settings));
	settings.serial.speed = speed;
        CHECK (gp_port_set_settings(camera->port, settings));

        DEBUG_PRINT_MEDIUM(("Baudrate set to: %i.", speed));

        return GP_OK;
}

int dsc1_getmodel(Camera *camera) {

        static const char       response[3] = { 'D', 'S', 'C' };

        DEBUG_PRINT_MEDIUM(("Getting camera model."));

        if (dsc1_sendcmd(camera, DSC1_CMD_GET_MODEL, NULL, 0) != GP_OK)
                return GP_ERROR;

        if (dsc1_retrcmd(camera) != DSC1_RSP_MODEL ||
                        memcmp(camera->pl->buf, response, 3) != 0)
                RETURN_ERROR(EDSCBADRSP);
                /* bad response */

        DEBUG_PRINT_MEDIUM(("Camera model is: %c.", camera->pl->buf[3]));

        switch (camera->pl->buf[3]) {
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

        int     i;

        fprintf(stderr, "\nMemory dump: size: %i, contents:\n", size);
        for (i = 0; i < size; i ++)
                fprintf(
                        stderr,
                        *((char*)buf + i) >= 32 &&
                        *((char*)buf + i) < 127 ? "%c" : "\\x%02x",
                        (uint8_t)*((char*)buf + i)
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

        va_list         pvar;
        static char     msg[512];

        va_start(pvar, format);
        vsprintf(msg, format, pvar);
        va_end(pvar);
        return msg;
}

void dsc_errorprint(int error, char *file, int line) {

	GP_DEBUG(
		"%s:%u: return code: %i, errno: %i, %s",
			file, line, error, errno, dsc_strerror(error));
}

/* End of dc.c */
