/****************************************************************************
 *
 * File: util.h
 *
 ****************************************************************************/

#ifndef _CANON_UTIL_H
#define _CANON_UTIL_H

/****************************************************************************
 *
 * prototypes
 *
 ****************************************************************************/

void dump_hex(Camera *camera,const char *msg, const unsigned char *buf, int len);
void debug_message(Camera *camera,const char * msg, ...);
void intatpos (unsigned char *block, int pos, int integer);
unsigned int get_int (const unsigned char *p);
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
