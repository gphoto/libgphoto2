/****************************************************************************
 *
 * File: util.h
 *
 * $Id$
 ****************************************************************************/

#ifndef _CANON_UTIL_H
#define _CANON_UTIL_H

#include <gphoto2-endian.h>

/****************************************************************************
 *
 * prototypes
 *
 ****************************************************************************/

void dump_hex(Camera *camera,const char *msg, const unsigned char *buf, int len);

int is_thumbnail (const char *name);
int is_image (const char *name);
int is_movie (const char *name);
int is_jpeg (const char *name);
int is_crw (const char *name);

int comp_dir (const void *a, const void *b);

#endif /* _CANON_UTIL_H */

/****************************************************************************
 *
 * End of file: util.h
 *
 ****************************************************************************/
