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

#define GP_MIME_RAW       "image/x-raw"
#define GP_MIME_PNG       "image/png"
#define GP_MIME_PNM       "image/pnm"
#define GP_MIME_PPM       "image/ppm"
#define GP_MIME_JPEG      "image/jpeg"
#define GP_MIME_TIFF      "image/tiff"
#define GP_MIME_QUICKTIME "video/quicktime"

typedef enum {
	GP_FILE_TYPE_PREVIEW,
	GP_FILE_TYPE_NORMAL,
	GP_FILE_TYPE_RAW
} CameraFileType;

typedef enum {
	GP_FILE_CONVERSION_METHOD_CHUCK,
	GP_FILE_CONVERSION_METHOD_JOHANNES
} CameraFileConversionMethod;

typedef struct _CameraFile CameraFile;

/* Don't use this - it'll disappear soon... */
struct _CameraFile {
	CameraFileType type;
	char mime_type [64];
	char name [64];
	long int size;
	unsigned char *data;
	int bytes_read;
	int session;
	int ref_count;

	unsigned char *red_table, *blue_table, *green_table;
	int red_size, blue_size, green_size;
	char header [128];
	int width, height;
	CameraFileConversionMethod method;
}; 

int gp_file_new            (CameraFile **file);
int gp_file_ref            (CameraFile *file);
int gp_file_unref          (CameraFile *file);

int gp_file_session        (CameraFile *file);

int gp_file_open           (CameraFile *file, char *filename);
int gp_file_save           (CameraFile *file, char *filename);

int gp_file_clean          (CameraFile *file);
int gp_file_free           (CameraFile *file);

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
