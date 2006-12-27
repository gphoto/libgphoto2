/****************************************************************************
 *
 * File: util.h
 *
 ****************************************************************************/

#ifndef _CANON_UTIL_H
#define _CANON_UTIL_H

#include "gphoto2-endian.h"

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/****************************************************************************
 *
 * prototypes
 *
 ****************************************************************************/
void dump_hex ( FILE *fp, void *buffer, int length );

int is_thumbnail (const char *name);
int is_audio (const char *name);
int is_image (const char *name);
int is_movie (const char *name);
int is_jpeg (const char *name);
int is_crw (const char *name);
int is_cr2 (const char *name);

const char *filename2mimetype(const char *filename);

#endif /* _CANON_UTIL_H */

/****************************************************************************
 *
 * End of file: util.h
 *
 ****************************************************************************/

/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
