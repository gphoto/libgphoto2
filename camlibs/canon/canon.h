/****************************************************************************
 *
 * File: canon.h
 *
 ****************************************************************************/

#ifndef _CANON_H
#define _CANON_H

/****************************************************************************
 *
 * global info structure
 *
 * Not much here yet...
 *
 ****************************************************************************/

typedef struct {
	gpio_device *dev;
	gpio_device_settings settings;
	int speed;
	int debug;
} CanonDataStruct;





/****************************************************************************
 *
 * These are defines for packet command codes collected from several
 * sources. There's no guarantee, that they are correct...
 *
 * The same is true for the ident string offset
 *
 ****************************************************************************/

#define CANON_IDENT_OFFSET		0x1a

#define CANON_CMD_ACK			0x04
#define CANON_CMD_PING			0x10

#define CANON_PCK_SOT			0x05
#define CANON_PCK_EOT			0x04
#define CANON_PCK_CMD			0x00
#define CANON_PCK_IDENT			0x06


void clear_readiness(Camera *camera);

#endif /* _CANON_H */

/****************************************************************************
 *
 * End of file: canon.h
 *
 ****************************************************************************/
