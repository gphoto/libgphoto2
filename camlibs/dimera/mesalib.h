/*
 * Copyright 2000-2001, Brian Beattie <beattie@aracnet.com>, et. al.
 *
 *	This software was created with the help of proprietary
 *      information belonging to StarDot Technologies
 *
 * Permission to copy and use and modify this file is granted
 * provided the above notices are retained, intact.
 *
 */

#ifndef MESALIB_H
#define MESALIB_H

#include <_stdint.h>
#include <gphoto2/gphoto2-port.h>

#define CHECK(result) {int res = (result); if (res < 0) return (res);}

/* MESA commands */
#define	NOP		0x01	/* No operation */
#define MESA_VERSION		0x05	/* Send Version command */
#define XMIT_TEST	0x09	/* Send test sequent */
#define RAM_TEST	0x0d	/* Test Camera RAM */
#define SEND_RAM	0x15	/* Send Ram contents */
#define SNAP_IMAGE	0x19	/* Snap an Image (no flash or shutter) */
#define SND_BLACK	0x1d	/* Send Black level bytes */
#define SNAP_VIEW	0x21	/* Snap (and Send) viewfinder image */
#define XTRA_STP_BIT	0x25	/* Stop bits (extra time between bytes */
#define SND_VIEW	0x29	/* Send viewfinder image */
#define SNAP_PICTURE	0x2d	/* Snap a picture */
#define	SND_ID		0x35	/* Send Manufacture/year/week of manufacture */
#define AT_COMMAND	0x41	/* Modem Detect command */
#define SND_IMAGE	0x49	/* Send image stored in RAM */
#define	RCV_TEST	0x4d	/* Echo six bytes */
#define IMAGE_CNT	0x55	/* Send count of images */
#define	LD_IMAGE	0x59	/* Load Image from EEPROM into RAM */
#define EEPROM_INFO	0x5d	/* Send information about EEPROM */
#define SND_THUMB	0x61	/* Send Thumbnail of image */
#define SET_SPEED	0x69	/* Set serial link speed */
#define FEATURES	0x6d	/* Send features */
#define SND_IMG_INFO	0x71	/* Send Image Info */

#define	CMD_ACK		0x21	/* Command ACK */

#define RAM_IMAGE_NUM	0x10000	/* image number for image in RAM */

/* RAM Test results */
#define RAM_GOOD	0x2a	/* RAM tests good */
#define RAM_ERROR	0x2d	/* RAM error detected */
#define EE_ERROR	0x45	/* Failure to restore EEPROM dir */

/* DownLoad ViewFinder */
#define ALL_LOWRES	0xff	/* Send all of a LowRES 64x48 viewfinder */
#define EVEN_LOWRES	0xfe	/* Send even rows of LowRES */
#define ODD_LOWRES	0xfd	/* Send odd rows of LowRES */
#define NONE		0xfc	/* Send nothing (for Snap Viewfinder) */
#define ALL_HIRES	0xfb	/* Send All of a HighRES 128x96 viewfinder */
#define EVEN_HIRES	0xfa	/* Send even rows of HiRES */
#define ODD_HIRES	0xf9	/* Send odd rows of HiRES */

/* Feature bits */
struct mesa_feature {
	uint8_t	feature_bits_lo;
#define HAVE_FLASH	0x01		/* Flash present */
#define HAVE_RES_SW	0x02		/* Resolution switch present */
#define FLASH_FILL	0x04		/* Flash is in Fill mode */
#define FLASH_READY	0x08		/* Flash is charged and ready */
#define LOW_RES		0x10		/* Switch is in low res position */
#define DUAL_IRIS	0x20		/* Dual Iris is present */
#define AC_PRESENT	0x40		/* AC Adapter is connected */
#define FLASH_ON	0x80		/* Flash is on */
	uint8_t	feature_bits_hi;
#define	IRIS_ADJUST	0x1f
#define BAT_VALID	0x20		/* Battery level valid */
#define NO_PWR_LIGHT	0x40		/* No power light */
#define	BAT_DIGITAL	0x80		/* Battery level is binary */
	uint8_t	battery_level;	/* Battery level */
	uint8_t	battery_zero;	/* Battery dead level */
	uint8_t	battery_full;	/* Battery full level */
};

/* Send Image */
struct mesa_image_arg {
	uint16_t	row;		/* row to start sending */
	uint16_t	start;		/* 0x1e */
	uint8_t	send;		/* 0x04 */
	uint8_t	skip;		/* 0x00 */
	uint16_t	repeat;		/* 0xA0 */
	uint8_t	row_cnt;	/* 0x20 */
	uint8_t	inc1;		/* 0x01 */
	uint8_t	inc2;		/* 0x80 */
	uint8_t	inc3;		/* 0x00 */
	uint8_t	inc4;		/* 0x00 */
};

struct mesa_id {
	uint16_t	man;	/* 12bit manufacturer ID */
	uint16_t	year;	/* year of manufacture - 1996 */
	uint8_t	ver;	/* 4bit version */
	uint8_t	week;	/* week of manufacture 0 - 51 */
};

#define MESA_THUMB_SZ	(64*60)	/* (64x60x4)/8 */

struct mesa_image_info {
	uint32_t	num_bytes;	/* size of image in eeprom */
	uint8_t	standard_res;	/* image is standard res */
};

#define MESA_VERSION_SZ 7	/* min. buffer length needed for mesa_version */
#define MESA_EEPROM_SZ 49	/* min. buffer length needed for mesa_eeprom_info */

void
mesa_flush( GPPort *port, int timeout );
int
mesa_read( GPPort *port, uint8_t *b, int s, int timeout2, int timeout1 );
int
mesa_send_command( GPPort *port, uint8_t *cmd, int n, int ackTimeout );
int
mesa_port_open( GPPort *port );
int
mesa_port_close( GPPort *port );
int
mesa_reset( GPPort *port );
int
mesa_set_speed( GPPort *port, int speed );
int
mesa_version( GPPort *port,char *version_string);
int
mesa_transmit_test( GPPort *port );
int
mesa_ram_test( GPPort *port );
int
mesa_read_row( GPPort *port, uint8_t *r, struct mesa_image_arg *s );
int
mesa_snap_image( GPPort *port, uint16_t exposure );
int
mesa_black_levels( GPPort *port, uint8_t r[2] );
int
mesa_snap_view( GPPort *port, uint8_t *r, unsigned int hi_res, unsigned int zoom,
	unsigned int row, unsigned int col, uint16_t exposure,
	uint8_t download );
int
mesa_set_stopbits( GPPort *port, unsigned int bits );
int
mesa_download_view( GPPort *port, uint8_t *r, uint8_t download );
int
mesa_snap_picture( GPPort *port, uint16_t exposure );
int
mesa_send_id( GPPort *port, struct mesa_id *id );
int
mesa_modem_check( GPPort *port );
int
mesa_read_image( GPPort *port, uint8_t *r, struct mesa_image_arg *s );
int
mesa_recv_test( GPPort *port, uint8_t r[6] );
int
mesa_get_image_count( GPPort *port );
int
mesa_load_image( GPPort *port, int image);
int
mesa_eeprom_info( GPPort *port, int long_read, uint8_t info[MESA_EEPROM_SZ] );
int32_t
mesa_read_thumbnail( GPPort *port, int picture, uint8_t *image );
int
mesa_read_features( GPPort *port, struct mesa_feature *f );
int
mesa_battery_check( GPPort *port );
int32_t
mesa_read_image_info( GPPort *port, int i, struct mesa_image_info *info );
uint8_t *
mesa_get_image( GPPort *port, int image );

#endif /* MESALIB_H */
