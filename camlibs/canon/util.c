/****************************************************************************
 *
 * File: util.c 
 *
 * Utility functions for the gPhoto Canon PowerShot A5(Z)/A50 driver.
 *
 * $Header$
 ****************************************************************************/

/****************************************************************************
 *
 * include files
 *
 ****************************************************************************/

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#include <gphoto2.h>

#include "gphoto2-port-log.h"
#include "util.h"
#include "psa50.h"

/*****************************************************************************
 *
 * dump_hex
 *
 * Dumps a memory area as hex on the screen.
 *
 * msg  - Info message for the dump
 * buf  - the memory buffer to dump
 * len  - length of the buffer
 *
 ****************************************************************************/

//extern int canon_debug_driver;

#define NIBBLE(_i)    (((_i) < 10) ? '0' + (_i) : 'A' + (_i) - 10)

void
dump_hex (Camera *camera, const char *msg, const unsigned char *buf, int len)
{
//      struct canon_info *cs = (struct canon_info*)camera->camlib_data;
	int i;
	int nlocal;
	const unsigned char *pc;
	char *out;
	const unsigned char *start;
	char c;
	char line[100];

	//   if (cs->debug > 8) { // Only  printout with max. debug level (9)
	start = buf;

#if 0
	if (len > 160) {
		fprintf (stderr, "dump n:%d --> 160\n", len);
		len = 160;
	}
#endif

	fprintf (stderr, "%s: (%d bytes)\n", msg, len);
	while (len > 0) {
		sprintf (line, "%08x: ", buf - start);
		out = line + 10;

		for (i = 0, pc = buf, nlocal = len; i < 16; i++, pc++) {
			if (nlocal > 0) {
				c = *pc;

				*out++ = NIBBLE ((c >> 4) & 0xF);
				*out++ = NIBBLE (c & 0xF);

				nlocal--;
			} else {
				*out++ = ' ';
				*out++ = ' ';
			}

			*out++ = ' ';
		}

		*out++ = '-';
		*out++ = ' ';

		for (i = 0, pc = buf, nlocal = len;
		     (i < 16) && (nlocal > 0); i++, pc++, nlocal--) {
			c = *pc;

			if ((c < ' ') || (c >= 126)) {
				c = '.';
			}

			*out++ = c;
		}

		*out++ = 0;

		fprintf (stderr, "%s\n", line);

		buf += 16;
		len -= 16;
	}			/* end while */
	/* } end 'if debug' */
}				/* end dump */

void
debug_message (Camera *camera, const char *msg, ...)
{
	va_list args;
        char buf[4096];

	/* We could stuff these instrution into a 
	 *      if (camera->pl->debug) { .... }
	 * statement, but this should be obsolete
	 * because we're using the official log message 
	 * mechanism anyway.
	 */
        va_start (args, msg);
        vsnprintf (buf, sizeof(buf), msg, args);
        va_end (args);

        gp_log (GP_LOG_DEBUG, "canon", "%s", buf);
}

/****************************************************************************
 *
 * End of file: util.c
 *
 ****************************************************************************/
