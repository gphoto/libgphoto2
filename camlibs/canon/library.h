/****************************************************************************
 *
 * File: library.h
 *
 * $Id$
 *
 ****************************************************************************/

#ifndef _LIBRARY_H
#define _LIBRARY_H

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


/****************************************************************************
 *
 * Canon file attributes
 *
 ****************************************************************************/

#define CANON_ATTR_WRITE_PROTECTED		0x01
#define CANON_ATTR_UNKNOWN_2			0x02
#define CANON_ATTR_UNKNOWN_4			0x04
#define CANON_ATTR_UNKNOWN_8			0x08			
#define CANON_ATTR_NON_RECURS_ENT_DIR		0x10
#define CANON_ATTR_DOWNLOADED			0x20
#define CANON_ATTR_UNKNOWN_40			0x40
#define CANON_ATTR_RECURS_ENT_DIR		0x80

void clear_readiness(Camera *camera);

#define GP_MODULE "canon"

#endif /* _CANON_H */

/****************************************************************************
 *
 * End of file: canon.h
 *
 ****************************************************************************/
