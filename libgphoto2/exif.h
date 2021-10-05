/** \file
 *
 * EXIF file format support library. This API allows to parse, read and
 * modify EXIF data structures. It provides a low-level API which allows
 * to manipulate EXIF tags in a generic way, and a higher-level API which
 * provides more advanced functions such as comment editing, thumbnail
 * extraction, etc.
 *
 * In the future, vendor-proprietary exif extensions might be supported.
 */

#ifndef LIBGPHOTO2_EXIF_H
#define LIBGPHOTO2_EXIF_H

/*
 * Not used anymore, use libexif if necessary.
 */
unsigned char *gpi_exif_get_thumbnail_and_size(void *exifdat, long *size);

/*
 * Not used anymore, use libexif if necessary.
 */
int gpi_exif_stat(void *exifdata);


#endif /* !defined(LIBGPHOTO2_EXIF_H) */
