/* gphoto2-file.h
 *
 * Copyright (C) 2000 Scott Fritzinger
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details. 
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GPHOTO2_FILE_H__
#define __GPHOTO2_FILE_H__

#define GP_MIME_WAV       "audio/wav"
#define GP_MIME_RAW       "image/x-raw"
#define GP_MIME_PNG       "image/png"
#define GP_MIME_PGM       "image/x-portable-graymap"
#define GP_MIME_PPM       "image/x-portable-pixmap"
#define GP_MIME_JPEG      "image/jpeg"
#define GP_MIME_TIFF      "image/tiff"
#define GP_MIME_BMP       "image/bmp"
#define GP_MIME_QUICKTIME "video/quicktime"
#define GP_MIME_AVI       "video/x-msvideo"
#define GP_MIME_CRW       "image/x-canon-raw"
#define GP_MIME_UNKNOWN   "application/octet-stream"

typedef enum {
	GP_FILE_TYPE_PREVIEW,
	GP_FILE_TYPE_NORMAL,
	GP_FILE_TYPE_RAW,
	GP_FILE_TYPE_AUDIO,
	GP_FILE_TYPE_EXIF
} CameraFileType;

/* Internals are private */
typedef struct _CameraFile CameraFile;

int gp_file_new            (CameraFile **file);
int gp_file_ref            (CameraFile *file);
int gp_file_unref          (CameraFile *file);
int gp_file_free           (CameraFile *file);

/* Do not use those */
int gp_file_open           (CameraFile *file, const char *filename);
int gp_file_save           (CameraFile *file, const char *filename);
int gp_file_clean          (CameraFile *file);

int gp_file_set_name       (CameraFile *file, const char *name);
int gp_file_set_mime_type  (CameraFile *file, const char *mime_type);
int gp_file_set_type       (CameraFile *file, CameraFileType type);

int gp_file_detect_mime_type          (CameraFile *file);
int gp_file_adjust_name_for_mime_type (CameraFile *file);

int gp_file_get_name       (CameraFile *file, const char **name);
int gp_file_get_mime_type  (CameraFile *file, const char **mime_type);
int gp_file_get_type       (CameraFile *file, CameraFileType *type);

int gp_file_append            (CameraFile*, const char *data,  int size);
int gp_file_get_last_chunk    (CameraFile*, char **data, long int *size);
int gp_file_set_data_and_size (CameraFile*, char *data, long int size);
int gp_file_get_data_and_size (CameraFile*, const char **data, long int *size);

int gp_file_copy           (CameraFile *destination, CameraFile *source);

/* Conversion */

/*
 * Please don't use the following in front-ends and camera drivers that are
 * not in gphoto CVS. We need to do some more work here, and this part of
 * the API is subject to change.
 *
 * If you like to do some work on conversion raw -> image/ *, please
 * step forward and write to gphoto-devel@gphoto.org.
 */
typedef enum {
	GP_FILE_CONVERSION_METHOD_CHUCK,
} CameraFileConversionMethod;

int gp_file_set_color_table  (CameraFile *file,
			      const unsigned char *red_table,   int red_size,
			      const unsigned char *green_table, int green_size,
			      const unsigned char *blue_table,  int blue_size);
int gp_file_set_width_and_height  (CameraFile *file, int width, int height);
int gp_file_set_header            (CameraFile *file, const char *header);
int gp_file_set_conversion_method (CameraFile *file,
				   CameraFileConversionMethod method);

int gp_file_convert (CameraFile *file, const char *mime_type);
	

#endif /* __GPHOTO2_FILE_H__ */
