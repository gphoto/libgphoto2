/*
* Copyright (C) 2000, Brian Beattie <beattie@aracnet.com>
*
*	This software was created with the help of proprietary
*      information belonging to StarDot Technologies
*
* Permission to copy and use and modify this file is granted
* provided the above notices are retained, intact.
*
* Revision History:
*	2001/03/10 - Port to gphoto2				  	  Dan
*
*	2000/09/06 - Modified by Gregg Berg <gberg@covconn.net>		  GDB
*		* Rewrite mesa_read_row() to use mesa_image_arg		  GDB
*		* Add checksum to mesa_read_row()			  GDB
*		* Add return with number of bytes to mesa_read_image()	  GDB
*									  GDB
* $Id$
*/

#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

#include "gphoto2.h"
#include "mesalib.h"

#define debuglog(s) gp_debug_printf(GP_DEBUG_LOW, "dimera", "%s", (s))

#ifdef CONVERT_PIXELS
static const u_int16_t	pixelTable[256] = {
	0x000, 0x001, 0x002, 0x003, 0x004, 0x005, 0x006, 0x007,
	0x008, 0x009, 0x00A, 0x00B, 0x00C, 0x00D, 0x00E, 0x00F,
	0x010, 0x011, 0x012, 0x013, 0x014, 0x015, 0x016, 0x017,
	0x018, 0x019, 0x01A, 0x01B, 0x01C, 0x01D, 0x01E, 0x01F,
	0x020, 0x021, 0x022, 0x023, 0x024, 0x025, 0x026, 0x027,
	0x028, 0x029, 0x02A, 0x02B, 0x02C, 0x02D, 0x02E, 0x02F,
	0x030, 0x031, 0x032, 0x033, 0x034, 0x035, 0x036, 0x037,
	0x038, 0x039, 0x03A, 0x03B, 0x03C, 0x03D, 0x03E, 0x03F,
	0x040, 0x041, 0x042, 0x043, 0x044, 0x045, 0x046, 0x047,

	0x048, 0x049, 0x04A, 0x04B, 0x04C, 0x04E, 0x04F, 0x050,
	0x051, 0x052, 0x053, 0x054, 0x055, 0x056, 0x057, 0x058,
	0x059, 0x05A, 0x05B, 0x05C, 0x05D, 0x05E, 0x05F, 0x060,
	0x061, 0x062, 0x063, 0x064, 0x066, 0x067, 0x068, 0x069,
	0x06A, 0x06C, 0x06D, 0x06E, 0x06F, 0x070, 0x072, 0x073,
	0x074, 0x075, 0x076, 0x077, 0x078, 0x079, 0x07A, 0x07B,
	0x07C, 0x07E, 0x07F, 0x081, 0x082, 0x084, 0x085, 0x087,

	0x088, 0x08A, 0x08B, 0x08C, 0x08E, 0x08F, 0x090, 0x092,
	0x093, 0x094, 0x096, 0x098, 0x09A, 0x09C, 0x09E, 0x0A0,
	0x0A2, 0x0A3, 0x0A5, 0x0A6, 0x0A8, 0x0A9, 0x0AB, 0x0AC,
	0x0AE, 0x0B0, 0x0B2, 0x0B4, 0x0B6, 0x0B8, 0x0BA, 0x0BC,

	0x0BE, 0x0C0, 0x0C2, 0x0C4, 0x0C6, 0x0C8, 0x0CA, 0x0CD,
	0x0CF, 0x0D2, 0x0D5, 0x0D8, 0x0DB, 0x0DE, 0x0E0, 0x0E2,
	0x0E5, 0x0E7, 0x0EA, 0x0EC, 0x0EE, 0x0F1, 0x0F3, 0x0F6,
	0x0FC, 0x102, 0x106, 0x10A, 0x10E, 0x111, 0x114, 0x117,

	0x11A, 0x11D, 0x120, 0x123, 0x126, 0x12C, 0x132, 0x138,
	0x13E, 0x142, 0x146, 0x14A, 0x150, 0x156, 0x15A, 0x15E,
	0x162, 0x166, 0x16A, 0x16E, 0x17A, 0x180, 0x186, 0x18C,
	0x192, 0x19E, 0x1AA, 0x1B0, 0x1B6, 0x1BA, 0x1BE, 0x1C2,

	0x1C8, 0x1CE, 0x1DA, 0x1E6, 0x1F2, 0x1FE, 0x204, 0x20A,
	0x216, 0x222, 0x22E, 0x23A, 0x240, 0x246, 0x24C, 0x252,

	0x25E, 0x26A, 0x276, 0x282, 0x28E, 0x2A6, 0x2B2, 0x2BE,
	0x2D6, 0x2EE, 0x2FA, 0x312, 0x336, 0x34E, 0x35e, 0x366
};
#endif

/*
 * return the difference in tenths of seconds, between t1 and t2.
 */
static long
timediff( struct timeval *t1, struct timeval *t2 )
{
	long	t;

	t = (t1->tv_sec - t2->tv_sec) * 10;
	t += (t1->tv_usec - t2->tv_usec) / 100000;
	return t;
}

/* flush input.  Wait for timeout deci-seconds to pass without input */

void
mesa_flush( gp_port *port, int timeout )
{
	u_int8_t	b[256];
	struct timeval	start, now;

	gettimeofday( &start, NULL );

	/* (df) Change this to use gp_port_flush */
	do {
		if ( gp_port_read( port, b, sizeof( b ) ) > 0 )
			/* reset timer */
			gettimeofday( &start, NULL );
		gettimeofday( &now, NULL );
	} while ( timeout > timediff( &now, &start ) );
	return;
}

/* Read exactly this number of bytes from the port within the given timeouts */
int
mesa_read( gp_port *port, u_int8_t *b, int s, int timeout2, int timeout1 )
{
	int		n = 0;
	int		r, t;
	struct timeval	start, now;

	t = timeout1 ? timeout1 : timeout2;	/* set first byte timeout */
	gettimeofday( &start, NULL );

	do
	{
		/* limit reads to 1k segment */
		r = gp_port_read( port, &b[n], s>1024?1024:s );
		if ( r > 0 )
		{
			n += r;
			s -= r;
			gettimeofday( &start, NULL );
			t = timeout2;
		}
		gettimeofday( &now, NULL );
	} while ( s > 0 && t > timediff( &now, &start ) );
	return n;
}

/* Send a command to the camera and read the acknowledgement byte */
int
mesa_send_command( gp_port *port, u_int8_t *cmd, int n, int ackTimeout )
{
	u_int8_t	c;

	if ( gp_port_write( port, cmd, n ) != GP_OK )
		return GP_ERROR_IO;

	if ( mesa_read( port, &c, 1, ackTimeout, 0 ) != 1 )
	{
		debuglog("mesa_send_command: timeout");
		return GP_ERROR_TIMEOUT;
	}
	if ( c != CMD_ACK )
	{
		debuglog("mesa_send_command: error response");
		return GP_ERROR_CORRUPTED_DATA;
	}
	return GP_OK;
}

/* Open the serial port and configure it */
int
mesa_port_open( gp_port **dev, char const *device )
{
	gp_port_settings settings;
	int ret;

	debuglog("mesa_port_open()");
	if ((ret = gp_port_new(dev, GP_PORT_SERIAL)) != GP_OK) {
		debuglog("mesa_port_open: error on gp_port_new");
		return (ret);
	}
	debuglog("mesa_port_open(): settings");
	gp_port_timeout_set(*dev, 5000);
	strcpy(settings.serial.port, device);

	settings.serial.speed   = 115200;
	settings.serial.bits    = 8;
	settings.serial.parity  = 0;
	settings.serial.stopbits= 1;

	if (gp_port_settings_set(*dev, settings) < 0)
		return GP_ERROR_BAD_PARAMETERS;

	if (gp_port_open(*dev) != GP_OK) {
		debuglog("mesa_port_open(): error selecting port settings");
		return GP_ERROR_IO;
	}

	return GP_OK;
}

/* Close the serial port */
int
mesa_port_close( gp_port *port )
{
	return gp_port_close(port);
}

/* reset camera */
int
mesa_reset( gp_port *port )
{
	return gp_port_send_break( port, 1 );
}

/* set camera serial port speed, then our local port speed */
int
mesa_set_speed( gp_port *port, int speed )
{
	u_int8_t	b[2];
	gp_port_settings settings;

	b[0] = SET_SPEED;

	switch( speed )
	{
	case 9600:
		b[1] = 1;
		break;
	case 14400:
		b[1] = 2;
		break;
	case 19200:
		b[1] = 3;
		break;
	case 38400:
		b[1] = 4;
		break;
	case 57600:
		b[1] = 5;
		break;
	case 76800:
		b[1] = 6;
		break;
	case 115200:
		b[1] = 7;
		break;
	case 230400:
		b[1] = 8;
		break;
	case 460800:
		b[1] = 9;
		break;
	default:
		return GP_ERROR_BAD_PARAMETERS;
	}
	if ( mesa_send_command( port, b, sizeof( b ), 10 ) != GP_OK )
	{
		return GP_ERROR;
	}

	gp_port_settings_get(port, &settings);
	settings.serial.speed = speed;
	gp_port_settings_set(port, settings);

	return GP_OK;
}

/* get camera version number */
int
mesa_version( gp_port *port, char *version_string)
{
	u_int8_t	b;
	u_int8_t	r[3];
	int		i;
	char	*v = version_string;

	b = MESA_VERSION;

	if ( mesa_send_command( port, &b, 1, 10 ) != GP_OK )
	{
		return GP_ERROR;
	}

	if ( ( i = mesa_read( port, r, sizeof( r ), 10, 0 ) ) != sizeof( r ) )
	{
		return GP_ERROR_TIMEOUT;
	}


	gp_debug_printf(GP_DEBUG_LOW, "dimera", 
	 "mesa_version: %02x:%02x:%02x\n", r[0], r[1], r[2] );
	v[0] = (r[1]&0xf0) ? ((r[1] >> 4)&15)+'0' : ' ';
	v[1] = (r[1] & 15)+'0';
	v[2] = '.';
	v[3] = ((r[0] >> 4)&15) + '0';
	v[4] = (r[0] & 15)+'0';
	v[5] = r[2];
	v[6] = 0;
	/* highest byte must be < MESA_VERSION_SZ */
	return GP_OK;
}

/* test camera serial port transmit */
int
mesa_transmit_test( gp_port *port )
{
	u_int8_t	b;
	u_int8_t	r[256];
	int		i;

	b = XMIT_TEST;

	if ( mesa_send_command( port, &b, 1, 10 ) != GP_OK )
	{
		return GP_ERROR;
	}

	if ( mesa_read( port, r, sizeof( r ), 10, 0 ) != sizeof( r ) )
	{
		return GP_ERROR_TIMEOUT;
	}

	for ( i = 0; i < 256; i++ )
	{
		if ( r[i] != i )
			return GP_ERROR_CORRUPTED_DATA;
	}
	return GP_OK;
}

/* test camera ram */
int
mesa_ram_test( gp_port *port )
{
	u_int8_t	b;
	u_int8_t	r;

	b = RAM_TEST;

	if ( mesa_send_command( port, &b, 1, 100 ) != GP_OK )
	{
		return GP_ERROR;
	}

	if ( mesa_read( port, &r, 1, 10, 0 ) != 1 )
	{
		return GP_ERROR_TIMEOUT;
	}

	return (int)r;
}

/*
 * Read image row from camera.
 * ********************************************************************   GDB
 * *   The following contradicted by traces from my Win98 twain driver*	  GDB
 * ********************************************************************   GDB
 * The image is stored in the camera as 504 row (0 - 503), the first 8 and the
 * last 2 are blank. 
 * The rows are 18 dummy pixels, 2 blanks, 646 pixels, 13 bytes garbage,
 * 2 bytes blanks
 *
 * start  - offset into row;
 * send   - bytes to send;
 * skip   - bytes to skip;
 * repeat - # repeats of send skip
 * 640 bytes start = 28, send = 2, skip = 0, repeat = 320
 * 320 bytes start = 28, send = 2, skip = 2, repeat = 160
 *
 * return number of bytes read, or < 0 if an error was detected.
 *									  GDB
 * ********************************************************************   GDB
 *									  GDB
 * 640 bytes start = 30, send = 4, skip = 0, repeat = 160		  GDB
 * 320 bytes start = 30, send = 4, skip = 0, repeat = 80		  GDB
 *									  GDB
 * return value	>0 number of bytes read					  GDB
 *		 GP_ERROR_TIMEOUT read time-out
 *		 GP_ERROR_BAD_PARAMETERS bad values
 *		 GP_ERROR command failure
 *		 GP_ERROR_CORRUPTED_DATA checksum failure
 */

int									/*GDB*/
mesa_read_row( gp_port *port, u_int8_t *r, struct mesa_image_arg *s )	/*GDB*/
{
	u_int8_t	b[9];
	int		bytes, i;					/*GDB*/
	u_int8_t	checksum = 0;					/*GDB*/

	if ( (bytes = s->send * s->repeat) > 680 )			/*GDB*/
	{
		return GP_ERROR_BAD_PARAMETERS;
	}

	b[0] = SEND_RAM;
	b[1] = s->row;							/*GDB*/
	b[2] = s->row>>8;						/*GDB*/
	b[3] = s->start;						/*GDB*/
	b[4] = s->start>>8;						/*GDB*/
	b[5] = s->send;							/*GDB*/
	b[6] = s->skip;							/*GDB*/
	b[7] = s->repeat;						/*GDB*/
	b[8] = s->repeat>>8;						/*GDB*/

	if ( mesa_send_command( port, b, sizeof( b ), 10 ) != GP_OK )
	{
		return GP_ERROR;
	}

/*	return ( mesa_read( port, r, n, 10, 0 ));			  GDB*/
	if ( mesa_read( port, r, bytes, 10, 0 ) != bytes )		/*GDB*/
	{								/*GDB*/
		return GP_ERROR_TIMEOUT;
	}								/*GDB*/
									/*GDB*/
	if ( mesa_read( port, b, 1, 10, 0 ) != 1) 			/*GDB*/
	{								/*GDB*/
		return GP_ERROR_TIMEOUT;
	}								/*GDB*/
									/*GDB*/
	for (i = 0; i < bytes; i++ )					/*GDB*/
	{								/*GDB*/
		checksum += r[i];					/*GDB*/
	}								/*GDB*/
									/*GDB*/
	if ( checksum != b[0] )						/*GDB*/
	{								/*GDB*/
		return GP_ERROR_CORRUPTED_DATA;
	}								/*GDB*/
									/*GDB*/
	return bytes;							/*GDB*/
}

/*
 * Snaps a full resolution image, with no flash, or shutter click
 *
 * exposure is in units of 1/50000 seconds
 */
int
mesa_snap_image( gp_port *port, u_int16_t exposure )
{
	u_int8_t	b[3];
	int		timeout;

	if ( exposure )
		timeout = (exposure/50000) + 10;
	else
		timeout = 10;

	b[0] = SNAP_IMAGE;
	b[1] = exposure;
	b[2] = exposure >> 8;
	return ( mesa_send_command( port, b, sizeof( b ), timeout ) );
}

/* return black levels, overwrites image memory */
int
mesa_black_levels( gp_port *port, u_int8_t r[2] )
{
	u_int8_t	b;

	b = SND_BLACK;

	if ( mesa_send_command( port, &b, 1, 10 ) != GP_OK )
	{
		return GP_ERROR;
	}

	return ( mesa_read( port, r, 2, 10, 0 ) );
}

/*
 * snap a view finder image, optionally return image in buffer.
 *
 * if hi_res is true, image is 128x96 packed 4 bits per pixel (6144 bytes).
 * else               image is 64x48 packed 4 bits per pixel (1536 bytes).
 *
 * the image can be full scale, 0, zoom 2x 1, zoom 4x, 2, and for
 * low res images, 8x, 3.
 *
 * row and column may be offset 0-197 and 0-223 for zoom. 
 *
 * exposure is * 1/50000 seconds.
 *
 * download specifies how, of if thge image is downloaded.
 * 0-47		returns one 32 byte (64 pixel) row.
 * 128-223	returns on 64 byte row.
 * 249		returns all odd rows of a hi-res image 1536 bytes.
 * 250		returns all even rows of a hi-res image 1536 bytes.
 * 251		returns complete hi res image 6144 bytes.
 * 252		returns nothing.
 * 253		returns all odd rows of low res image 768 bytes.
 * 254		returns all even rows of low res image 768 bytes.
 * 255		returns entire image of 1536 bytes.
 *
 */
int
mesa_snap_view( gp_port *port, u_int8_t *r, unsigned int hi_res,
	unsigned int zoom, unsigned int row, unsigned int col, u_int16_t exposure,
	u_int8_t download )
{
	unsigned int	timeout, i;
	unsigned int	bytes = 0;
	u_int8_t	b[7], cksum;

	if ( download <= 47 )
		bytes = 32;
	else if ( download >= 48 && download <= 127 )
		return GP_ERROR_BAD_PARAMETERS;
	else if ( download >= 128 && download <= 223 )
		bytes = 64;
	else if ( download >= 224 && download <= 248 )
		return GP_ERROR_BAD_PARAMETERS;
	else if ( download == 249 || download == 250 )
		bytes = 1536;
	else if ( download == 251 ) 
		bytes = 6144;
	else if ( download == 252 )
		bytes = 0;
	else if ( download == 253 || download == 254 )
		bytes = 768;
	else /* if ( download == 255 ) */
		bytes = 1536;

	if ( bytes != 0 && r == NULL )
	{
		return GP_ERROR_BAD_PARAMETERS;
	}

	if ( exposure )
		timeout = (exposure/50000) + 10;
	else
		timeout = 10;

	b[0] = SNAP_VIEW;
	b[1] = (zoom&3)+(hi_res?0x80:0);
	b[2] = row;
	b[3] = col;
	b[4] = exposure;
	b[5] = exposure >> 8;
	b[6] = download;

	if ( mesa_send_command( port, b, sizeof( b ), timeout ) != GP_OK )
	{
		return GP_ERROR;
	}

	if ( bytes != 0 )
	{
		if ( mesa_read( port, r, bytes, 10, 0 ) != bytes )
		{
			return GP_ERROR_TIMEOUT;
		}

		if ( mesa_read( port, b, 1, 10, 0 ) != 1 )
		{
			return GP_ERROR_TIMEOUT;
		}

		for ( i = 0, cksum = 0; i < bytes; i++ )
		{
			cksum += r[i];
		}
		if ( cksum != b[0] )
		{
			return GP_ERROR_CORRUPTED_DATA;
		}
	}
	
	return bytes;
}

/*
 * tell camera to insert extra stop bits between bytes.
 * slows down transmission without using a slower speed.
 */
int
mesa_set_stopbits( gp_port *port, unsigned int bits )
{
	u_int8_t	b[2];

	b[0] = XTRA_STP_BIT;
	b[1] = bits;

	return ( mesa_send_command( port, b, sizeof( b ), 10 ) );
}

/* download viewfinder image ( see mesa_snap_view ) */
int
mesa_download_view( gp_port *port, u_int8_t *r, u_int8_t download )
{
	unsigned int	bytes, i;
	u_int8_t	b[2], cksum;

	if ( download <= 47 )
		bytes = 32;
	else if ( download >= 48 && download <= 127 )
		return GP_ERROR_BAD_PARAMETERS;
	else if ( download >= 128 && download <= 223 )
		bytes = 64;
	else if ( download >= 224 && download <= 248 )
		return GP_ERROR_BAD_PARAMETERS;
	else if ( download == 249 )
		bytes = 1536;
	else if ( download == 250 || download == 251 ) 
		bytes = 768;
	else if ( download == 252 )
		bytes = 0;
	else if ( download == 253 )
		bytes = 6144;
	else /* if ( download == 254 || download == 255 ) */
		bytes = 1536;

	if ( bytes != 0 && r == NULL )
	{
		return GP_ERROR_BAD_PARAMETERS;
	}

	b[0] = SND_VIEW;
	b[1] = download;

	if ( mesa_send_command( port, b, sizeof( b ), 10 ) != GP_OK )
	{
		return GP_ERROR;
	}

	if ( bytes != 0 )
	{
		if ( mesa_read( port, r, bytes, 10, 0 ) != bytes )
		{
			return GP_ERROR_TIMEOUT;
		}

		if ( mesa_read( port, b, 1, 10, 0 ) != 1 )
		{
			return GP_ERROR_TIMEOUT;
		}

		for ( i = 0, cksum = 0; i < bytes; i++ )
		{
			cksum += r[i];
		}
		if ( cksum != b[0] )
			return GP_ERROR_CORRUPTED_DATA;
	}
	
	return bytes;
}

/*
 * take a picture, with flash and shutter click
 */
int
mesa_snap_picture( gp_port *port, u_int16_t exposure )
{
	unsigned int	timeout;
	u_int8_t	b[3];

	if ( exposure )
		timeout = (exposure/50000) + 10;
	else
		timeout = 10;

	b[0] = SNAP_PICTURE;
	b[1] = exposure;
	b[2] = exposure>>8;

	return ( mesa_send_command( port, b, sizeof( b ), timeout ) );
}

/* Get the camera chipset identification */
int
mesa_send_id( gp_port *port, struct mesa_id *id )
{
	u_int8_t	b, r[4];

	b = SND_ID;

	if ( mesa_send_command( port, &b, 1, 10 ) != GP_OK )
	{
		return GP_ERROR;
	}

	if ( mesa_read( port, r, sizeof( r ), 10, 0 ) != sizeof( r ) )
	{
		return GP_ERROR_TIMEOUT;
	}

	id->man = r[0] + ((r[1]&15)<<8);
	id->ver = r[1]>>4;
	id->year = 1996 + r[2];
	id->week = r[3];

	return GP_OK;
}

/*
 * check for a camera, or a modem, send AT, if the response is 0x21,
 * it is a camera, if it is "AT" it's a modem.
 *
 * returns	 GP_OK = camera
 *		 GP_ERROR_MODEL_NOT_FOUND = modem
 *		 other = error
 */
int
mesa_modem_check( gp_port *port )
{
	u_int8_t b[3];

	b[0] = 'A';
	b[1] = 'T';
	b[2] = '\r';

	if ( gp_port_write( port, b, sizeof( b ) ) != GP_OK )
		return GP_ERROR_IO;

	/* Expect at least one byte */
	if ( mesa_read( port, b, 1, 5, 0 ) < 1 )
	{
		return GP_ERROR_TIMEOUT;
	}

	if ( b[0] == CMD_ACK )
		return GP_OK;

	/* Anything past this point results in an error */
	mesa_read( port, b+1, sizeof(b) - 1, 2, 2 );

	if ( b[0] == 'A' && b[1] == 'T' )
	{
		mesa_flush( port, 10 );
		return GP_ERROR_MODEL_NOT_FOUND;
	}

	mesa_flush( port, 10 );
	return GP_ERROR;
}

/*
 * mesa_read_image	- returns all or part of an image in the supplied buffer
 * 	u_int8_t		*r, buffer to return image in.
 *	struct mesa_image_arg	*i, download specifier.
 *
 *	return value	>0 number of bytes read
 *			GP_ERROR_TIMEOUT read time-out
 *			GP_ERROR command failure
 *			GP_ERROR_CORRUPTED_DATA checksum failure
 */

int
mesa_read_image( gp_port *port, u_int8_t *r, struct mesa_image_arg *s )
{
	u_int8_t	b[14];
	int		bytes, i;
	u_int8_t	checksum = 0;

	bytes = s->row_cnt * (s->repeat * s->send);

	b[0]  = SND_IMAGE;
	b[1]  = s->row;
	b[2]  = s->row>>8;
	b[3]  = s->start;
	b[4]  = s->start>>8;
	b[5]  = s->send;
	b[6]  = s->skip;
	b[7]  = s->repeat;
	b[8]  = s->repeat>>8;
	b[9]  = s->row_cnt;
	b[10] = s->inc1;
	b[11] = s->inc2;
	b[12] = s->inc3;
	b[13] = s->inc4;

	if ( mesa_send_command( port, b, sizeof( b ), 10 ) != GP_OK )
	{
		return GP_ERROR;
	}

	if ( mesa_read( port, r, bytes, 10, 0 ) != bytes )
	{
		return GP_ERROR_TIMEOUT;
	}

	if ( mesa_read( port, b, 1, 10, 0 ) != 1 )
	{
		return GP_ERROR_TIMEOUT;
	}

	for ( i = 0; i < bytes; i++ )
	{
		checksum += r[ i ];
	}

	if ( checksum != b[0] )
	{
		return GP_ERROR_CORRUPTED_DATA;
	}

	return bytes;
}

/*
 * test serial link, send six bytes
 *
 * returns	GP_OK - success
 *		GP_ERROR_TIMEOUT - no data received
 *		GP_ERROR - command failed (no or bad ack)
 *		GP_ERROR_CORRUPTED_DATA - returned bytes do not match
 */
int
mesa_recv_test( gp_port *port, u_int8_t r[6] )
{
	u_int8_t	b[7];
	int		i;
	
	b[0] = RCV_TEST;
	memcpy( &b[1], r, 6 );

	if ( mesa_send_command( port, b, sizeof( b ), 10 ) != GP_OK )
	{
		return GP_ERROR;
	}

	if ( mesa_read( port, r, 6, 10, 0 ) != 6 )
	{
		return GP_ERROR_TIMEOUT;
	}

	for ( i = 0; i < 6; i++ )
		if ( r[i] != b[i+1] )
			return GP_ERROR_CORRUPTED_DATA;
	return GP_OK;
}

/* Return the number of images in the EEPROM */
int
mesa_get_image_count( gp_port *port )
{
	u_int8_t	b;
	u_int8_t	r[2];

	b = IMAGE_CNT;

	if ( mesa_send_command( port, &b, 1, 10 ) != GP_OK )
	{
		return GP_ERROR;
	}

	if ( mesa_read( port, r, sizeof( r ), 10, 0 ) != sizeof( r ) )
	{
		return GP_ERROR_TIMEOUT;
	}

	return ( r[0] + (r[1]<<8) );
}

/* load image from eeprom, into ram */
int
mesa_load_image( gp_port *port, int image )
{
	u_int8_t	b[3];

	b[0] = LD_IMAGE;
	b[1] = image;
	b[2] = image >> 8;

	return ( mesa_send_command( port, b, sizeof( b ), 1000 ) );
}

/*
 * Read EEPROM information
 * Bytes 10..16 of info correspond to the Device ID field, bytes 45..51
 */
int
mesa_eeprom_info( gp_port *port, int long_read, u_int8_t info[MESA_EEPROM_SZ] )
{
	u_int8_t	b;

	b = EEPROM_INFO;

	if ( mesa_send_command( port, &b, 1, 10 ) != GP_OK )
	{
		return GP_ERROR;
	}

	return ( mesa_read( port, info, long_read ? 49 : 33, 10, 0 ) );
}

/*
 *
 */

int32_t
mesa_read_thumbnail( gp_port *port, int picture, u_int8_t *image )
{
	u_int8_t	b[3], checksum = 0;
	u_int32_t	bytes;
	int		standard_res, i;

	b[0] = SND_THUMB;
	b[1] = picture&255;
	b[2] = (picture>>8)&255;

	if ( mesa_send_command( port, b, sizeof( b ), 10 ) != GP_OK )
	{
		return GP_ERROR;
	}

	if ( mesa_read( port, b, 3, 10, 0 ) != 3 )
	{
		return GP_ERROR_TIMEOUT;
	}

	checksum = b[0] + b[1] + b[2];

	standard_res = ((b[2] & 128) != 0);
	bytes = b[0] + (b[1]<<8) + ((b[2] & 127)<<16);

	if ( mesa_read( port, image, MESA_THUMB_SZ, 10, 0 ) != MESA_THUMB_SZ )
	{
		return GP_ERROR_TIMEOUT;
	}

	if ( mesa_read( port, b, 1, 10, 0 ) != 1 )
	{
		return GP_ERROR_TIMEOUT;
	}

	for ( i = 0; i < MESA_THUMB_SZ; i++ )
	{
		checksum += image[i];
	}

	if ( checksum != b[0] )
		return GP_ERROR_CORRUPTED_DATA;

	return (bytes + standard_res ? 0x1000000 : 0);
}

/* Return the camera's feature bits */
int
mesa_read_features( gp_port *port, struct mesa_feature *f )
{
	u_int8_t	b;

	b = FEATURES;

	if ( mesa_send_command( port, &b, 1, 10 ) != GP_OK )
	{
		return GP_ERROR;
	}

	return ( mesa_read( port, (u_int8_t *)f, sizeof( *f ), 10, 0 ) );
}

/* return percentage battery level */
int
mesa_battery_check( gp_port *port )
{
	struct mesa_feature	f;
	int			l, r;

	if ( mesa_read_features( port, &f ) != sizeof( f ))
	{
		return GP_ERROR;
	}

	if ( (f.feature_bits_hi & BAT_VALID) == 0 )
		return GP_ERROR;

	/* (df) Does BAT_DIGITAL need to be checked here? */

	if ( ( l =  f.battery_level - f.battery_zero ) < 0 )
		l = 0;

	r = f.battery_full - f.battery_zero;

	return ( (l*100)/r );
}

int32_t
mesa_read_image_info( gp_port *port, int i, struct mesa_image_info *info )
{
	u_int8_t	b[3], r[3];
	int32_t		bytes;
	int		standard_res;

	b[0] = SND_IMG_INFO;
	b[1] = i & 255;
	b[2] = (i >> 8)&255;

	if ( mesa_send_command( port, b, sizeof( b ), 10 ) != GP_OK )
	{
		return GP_ERROR;
	}

	if ( mesa_read( port, r, sizeof( r ), 10, 0 ) != sizeof( r ) )
	{
		return GP_ERROR_TIMEOUT;
	}

	standard_res = ((r[2] & 128) != 0);
	bytes = r[0] + (r[1]<<8) + ((r[2] & 127)<<16);

	if ( info != NULL )
	{
		info->standard_res = standard_res;
		info->num_bytes = bytes;
	}

	return standard_res;
}

#ifdef CONVERT_PIXELS
/*
 * return and image with raw pixels expanded.
 */
u_int16_t *
mesa_get_image( gp_port *port, int image )
{
	static u_int8_t			buffer[307200];
	static struct mesa_image_info	info;
	static struct mesa_image_arg	ia;
	u_int8_t			*b = buffer;
	u_int16_t			*rbuffer;
	int				r, size, res, i;

	if ( image != RAM_IMAGE_NUM )
	{
		if ( mesa_load_image( port, image ) != GP_OK )
		{
			mesa_flush( port, 100 );
			return NULL;
		}

		if ( mesa_read_image_info( port, image, &info ) < 0 )
		{
			mesa_flush( port, 100 );
			return NULL;
		}
		if ( info.standard_res )
		{
			res = 1;
			size = 320*240;
		} else {
			res = 0;
			size = 640*480;
		}
	} else {
		res = 0;
		size = 640*480;
	}
	rbuffer = (u_int16_t *)malloc( size*(sizeof (u_int16_t)) );
	if ( rbuffer == NULL )
		return NULL;

	ia.start = 28;
	ia.send = 4;
	ia.skip = 0;
	ia.repeat = (res ? 80 : 160);
	ia.row_cnt = 40;
	ia.inc1 = 1;
	ia.inc2 = 128;
	ia.inc3 = ia.inc4 = 0;

	for ( ia.row = 4; ia.row < (res ? 244 : 484) ; ia.row += ia.row_cnt )
	{

		if ( (r = mesa_read_image( port, b, &ia )) < 0 )
		{
			free( rbuffer );
			return NULL;
		}
		b += r;
	}

	for ( i = 0; i < size; i++ )
	{
		*rbuffer++ = pixelTable[*b++];
	}

	return rbuffer;
}
#else
/*
 * return and raw image retransmit on errors
 */
u_int8_t *
mesa_get_image( gp_port *port, int image )
{
	static struct mesa_image_info	info;
	static struct mesa_image_arg	ia;
	u_int8_t			*rbuffer, *b;
	int				r, size, res;
	int				retry;

	if ( image != RAM_IMAGE_NUM )
	{
		if ( mesa_load_image( port, image ) < 0 )
		{
			mesa_flush( port, 100 );
			return NULL;
		}

		if ( mesa_read_image_info( port, image, &info ) < 0 )
		{
			mesa_flush( port, 100 );
			return NULL;
		}
		if ( info.standard_res )
		{
			res = 1;
			size = 320*240;
		} else {
			res = 0;
			size = 640*480;
		}
	} else {
		res = 0;
		size = 640*480;
	}
	rbuffer = (u_int8_t *)malloc( size );
	if ( rbuffer == NULL )
		return NULL;
	b = rbuffer;

	ia.start = 28;
	ia.send = 4;
	ia.skip = 0;
	ia.repeat = (res ? 80 : 160);
	ia.row_cnt = 40;
	ia.inc1 = 1;
	ia.inc2 = 128;
	ia.inc3 = ia.inc4 = 0;

	for ( ia.row = 4; ia.row < (res ? 244 : 484) ; ia.row += ia.row_cnt )
	{

		for ( retry = 10;; )
		{
			if ( (r = mesa_read_image( port, b, &ia )) > 0)
				break;

				/* if checksum error count retries */
			if ( r == -2  && --retry > 0)
				continue;	/* not exceeded, try again */

				/* error, exit */
			free( rbuffer );
			return NULL;

		}
		b += r;
	}

	return rbuffer;
}
#endif
