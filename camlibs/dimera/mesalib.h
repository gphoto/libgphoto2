/*
 * Copyright (C) 2000-2001, Brian Beattie <beattie@aracnet.com>, et. al.
 *
 *	This software was created with the help of proprietary
 *      information belonging to StarDot Technologies
 *
 * Permission to copy and use and modify this file is granted
 * provided the above notices are retained, intact.
 *
 * $Id$
 *
 * History:
 * $Log$
 * Revision 1.4  2001/10/16 18:45:10  dfandrich
 * Added CHECK macro to ease error handling
 *
 * Revision 1.3  2001/09/10 20:25:44  dfandrich
 * Added missing types for Solaris
 *
 * Revision 1.2  2001/08/29 21:57:28  dfandrich
 * Changed port parameter to mesa_port_open
 *
 */

#ifndef MESALIB_H
#define MESALIB_H

/* Solaris doesn't have the u_int*_t types, so define them here. */
/* The check for this should really be in the autoconf script, not here */
#ifdef sun
typedef uint8_t u_int8_t;
typedef uint16_t u_int16_t;
typedef uint32_t u_int32_t;
#endif

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
	u_int8_t	feature_bits_lo;
#define HAVE_FLASH	0x01		/* Flash present */
#define HAVE_RES_SW	0x02		/* Resolution switch present */
#define FLASH_FILL	0x04		/* Flash is in Fill mode */
#define FLASH_READY	0x08		/* Flash is charged and ready */
#define LOW_RES		0x10		/* Switch is in low res position */
#define DUAL_IRIS	0x20		/* Dual Iris is present */
#define AC_PRESENT	0x40		/* AC Adapter is connected */
#define FLASH_ON	0x80		/* Flash is on */
	u_int8_t	feature_bits_hi;
#define	IRIS_ADJUST	0x1f
#define BAT_VALID	0x20		/* Battery level valid */
#define NO_PWR_LIGHT	0x40		/* No power light */
#define	BAT_DIGITAL	0x80		/* Battery level is binary */
	u_int8_t	battery_level;	/* Battery level */
	u_int8_t	battery_zero;	/* Battery dead level */
	u_int8_t	battery_full;	/* Battery full level */
};

/* Send Image */
struct mesa_image_arg {
	u_int16_t	row;		/* row to start sending */
	u_int16_t	start;		/* 0x1e */
	u_int8_t	send;		/* 0x04 */
	u_int8_t	skip;		/* 0x00 */
	u_int16_t	repeat;		/* 0xA0 */
	u_int8_t	row_cnt;	/* 0x20 */
	u_int8_t	inc1;		/* 0x01 */
	u_int8_t	inc2;		/* 0x80 */
	u_int8_t	inc3;		/* 0x00 */
	u_int8_t	inc4;		/* 0x00 */
};

struct mesa_id {
	u_int16_t	man;	/* 12bit manufacturer ID */
	u_int16_t	year;	/* year of manufacture - 1996 */
	u_int8_t	ver;	/* 4bit version */
	u_int8_t	week;	/* week of manufacture 0 - 51 */
};

#define MESA_THUMB_SZ	(64*60)	/* (64x60x4)/8 */

struct mesa_image_info {
	u_int32_t	num_bytes;	/* size of image in eeprom */
	u_int8_t	standard_res;	/* image is standard res */
};

#define MESA_VERSION_SZ 7	/* min. buffer length needed for mesa_version */
#define MESA_EEPROM_SZ 49	/* min. buffer length needed for mesa_eeprom_info */

void
mesa_flush( gp_port *port, int timeout );
int
mesa_read( gp_port *port, u_int8_t *b, int s, int timeout2, int timeout1 );
int
mesa_send_command( gp_port *port, u_int8_t *cmd, int n, int ackTimeout );
int
mesa_port_open( gp_port *port, char const *device );
int
mesa_port_close( gp_port *port );
int
mesa_reset( gp_port *port );
int
mesa_set_speed( gp_port *port, int speed );
int
mesa_version( gp_port *port,char *version_string);
int
mesa_transmit_test( gp_port *port );
int
mesa_ram_test( gp_port *port );
int
mesa_read_row( gp_port *port, u_int8_t *r, struct mesa_image_arg *s );
int
mesa_snap_image( gp_port *port, u_int16_t exposure );
int
mesa_black_levels( gp_port *port, u_int8_t r[2] );
int
mesa_snap_view( gp_port *port, u_int8_t *r, unsigned int hi_res, unsigned int zoom,
	unsigned int row, unsigned int col, u_int16_t exposure,
	u_int8_t download );
int
mesa_set_stopbits( gp_port *port, unsigned int bits );
int
mesa_download_view( gp_port *port, u_int8_t *r, u_int8_t download );
int
mesa_snap_picture( gp_port *port, u_int16_t exposure );
int
mesa_send_id( gp_port *port, struct mesa_id *id );
int
mesa_modem_check( gp_port *port );
int
mesa_read_image( gp_port *port, u_int8_t *r, struct mesa_image_arg *s );
int
mesa_recv_test( gp_port *port, u_int8_t r[6] );
int
mesa_get_image_count( gp_port *port );
int
mesa_load_image( gp_port *port, int image);
int
mesa_eeprom_info( gp_port *port, int long_read, u_int8_t info[MESA_EEPROM_SZ] );
int32_t
mesa_read_thumbnail( gp_port *port, int picture, u_int8_t *image );
int
mesa_read_features( gp_port *port, struct mesa_feature *f );
int
mesa_battery_check( gp_port *port );
int32_t
mesa_read_image_info( gp_port *port, int i, struct mesa_image_info *info );
u_int8_t *
mesa_get_image( gp_port *port, int image );

#endif /* MESALIB_H */
