/****************************************************************************
 *
 * File: library.h
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

/* #define CANON_CMD_ACK			0x04 */
/* #define CANON_CMD_PING			0x10 */

/* #define CANON_PCK_SOT			0x05 */
/* #define CANON_PCK_EOT			0x04 */
/* #define CANON_PCK_CMD			0x00 */
/* #define CANON_PCK_IDENT			0x06 */


void clear_readiness(Camera *camera);

#define GP_MODULE "canon"

#endif /* _LIBRARY_H */

/****************************************************************************
 *
 * End of file: library.h
 *
 ****************************************************************************/

/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
