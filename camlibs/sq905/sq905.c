/*
 * sq905.c
 *
 * Copyright (c) 2003 Theodore Kilgore <kilgota@auburn.edu>
 * Camera library support under libgphoto2.1.1 for camera(s)
 * with chipset from Service & Quality Technologies, Taiwan.
 * The chip supported by this driver is presumed to be the SQ905,
 *
 * Licensed under GNU Lesser General Public License, as part of Gphoto
 * camera support project. For a copy of the license, see the file
 * COPYING in the main source tree of libgphoto2.
 *
 * Helpful hints obtained by looking at several other Gphoto camera libs.
 * Thanks to Kelly Price, network admin for the Department of Computer
 * Science and Software Engineering, Auburn Unioversity. for some basic
 * help with C.
 */
#include <config.h>
#include "sq905.h"

#include <stdlib.h>
#include <string.h>

#include <libgphoto2_port/gphoto2-port-result.h>

#define SQ905_PING "\x00"
#define SQ905_GET  "\xc1"

#define SQWRITE gp_port_usb_msg_write
#define SQREAD  gp_port_usb_msg_read

unsigned int
sq_config_get_num_pic (SQConfig data)
{
	unsigned int i;

	for (i = 0; i * 0x10 < 0x4000; i++)
		if (!data[i * 0x10]) return i;
	return 0;
}

unsigned int
sq_config_get_width (SQConfig data, unsigned int n)
{
	switch (data[n * 0x10]) {
	case 0x41:
	case 0x61:
		return 352;
	case 0x42:
	case 0x62:
		return 176;
	default:
		return 0;
	}
}

unsigned int
sq_config_get_height (SQConfig data, unsigned int n)
{
	switch (data[n * 0x10]) {
	case 0x41:
	case 0x61:
		return 288;
	case 0x42:
	case 0x62:
		return 144;
	default:
		return 0;
	}
}

unsigned int
sq_config_get_comp (SQConfig data, unsigned int n)
{
	switch (data[n * 0x10]) {
	case 0x41:
	case 0x61:
		return 1;
	case 0x42:
	case 0x62:
		return 2;
	default:
		return 0;
	}
}

int
sq_init (GPPort *port, SQConfig data)
{
	char c[5];
	char msg[0x4000];
	unsigned int i;

	/*
	 * We throw the setup data away the first time around, because it is
	 * very likely corrupted, and we run through the whole sequence again,
	 * keeping the configuration data the second time until the point
	 * where we got all of the actually usable data out.
	 */
	for (i = 0; i < 2; i++) {

		SQWRITE (port, 0x0c, 0x06, 0xf0, SQ905_PING, 1);
		SQREAD  (port, 0x0c, 0x07, 0x00, &c[0], 5);
		SQWRITE (port, 0x0c, 0x06, 0xa0, &c[0], 1);
		SQREAD  (port, 0x0c, 0x07, 0x00, &c[0], 1);

		SQWRITE (port, 0x0c, 0x06, 0xf0, SQ905_PING, 1);
		SQREAD  (port, 0x0c, 0x07, 0x00, &c[0], 1);
		SQWRITE (port, 0x0c, 0x03, 0x04, SQ905_GET, 1);

		/*
		 * We put a four bytes coin in the camera. What is in the 
		 * four bytes seems absolutely not to matter, but to be safe
		 * we made sure they are all zero.
		 */
		memset (c, 0, sizeof (c));
		gp_port_write (port, c, 4);
		gp_port_read  (port, c, 4);

		/*
		 * If all is OK we receive "09 05 00 26"  Translates to "905 &"
		 * Seems that this sequence aligns the memory pointer. Since
		 * it doesn't always come out right the first time, though,
		 * setup routine is done twice.
		 */
		SQWRITE (port, 0x0c, 0xc0, 0x00, SQ905_GET, 1);
		SQWRITE (port, 0x0c, 0x06, 0xa0, SQ905_PING, 1);
		SQREAD  (port, 0x0c, 0x07, 0x00, &c[0], 1);
		SQWRITE (port, 0x0c, 0x06, 0x20, SQ905_PING, 1);
		SQREAD  (port, 0x0c, 0x07, 0x00, &c[0], 1);
		SQWRITE (port, 0x0c, 0x03, 0x4000, SQ905_GET, 1);

		/*
		 * Put coin in camera, worth 0x4000 bytes, which is the
		 * configuration data plus a lot of junk. Then kick the camera.
		 */
		memset (msg, 0, sizeof (msg));
		gp_port_write (port, msg, 0x4000);
		gp_port_read (port, &data[0], 0x4000);

		SQWRITE (port, 0x0c, 0xc0, 0x00, SQ905_GET, 1);
		SQWRITE (port, 0x0c, 0x06, 0xa0, SQ905_PING, 1);
		SQREAD  (port, 0x0c, 0x07, 0x00, &c[0], 1);
		SQWRITE (port, 0x0c, 0xc0, 0x00, SQ905_PING, 1);
		SQWRITE (port, 0x0c, 0x06, 0x30, SQ905_PING, 1);
		SQREAD  (port, 0x0c, 0x07, 0x00, &c[0], 1);
	}

	return GP_OK;
}
