/****************************************************************************
 *
 * File: util.h
 *
 * $Id$
 ****************************************************************************/

#ifndef _CANON_UTIL_H
#define _CANON_UTIL_H

#include <gphoto2-endian.h>

#ifndef MAX
# define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

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

const char *filename2mimetype(const char *filename);

int comp_dir (const void *a, const void *b);

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
