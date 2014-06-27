/* Sony DSC-F55 & MSAC-SR1 - gPhoto2 camera library
 * Copyright 2001, 2002, 2004 Raymond Penners <raymond@dotsphinx.com>
 * Copyright 2000 Mark Davies <mdavies@dial.pipex.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */
#ifndef SONY_H
#define SONY_H

#define SONY_CAMERA_ID "sonydscf55"

typedef enum {
	SONY_MODEL_MSAC_SR1 = 0,
	SONY_MODEL_DCR_PC100,
	SONY_MODEL_TRV_20E,
	SONY_MODEL_DSC_F55,
	SONY_MODEL_SIZEOF
} SonyModel;

#define SONY_FILE_NAME_FMT "dsc%05d.jpg"

typedef struct _tagPacket {
	int valid;
	int length;
	unsigned char buffer[16384];
	unsigned char checksum;
} Packet;

struct _CameraPrivateLibrary {
	unsigned short int sequence_id;
	long current_baud_rate;
	int current_mpeg_mode;
	SonyModel model;
};


typedef enum
{
	SONY_FILE_EXIF=0,
	SONY_FILE_THUMBNAIL,
	SONY_FILE_IMAGE,
	SONY_FILE_MPEG
} SonyFileType;

int sony_init(Camera * camera, SonyModel model);
int sony_exit(Camera * camera);
int sony_file_count(Camera * camera, SonyFileType file_type, int * count);
int sony_image_get(Camera * camera, int imageid, CameraFile * file, GPContext *context);
int sony_thumbnail_get(Camera * camera, int imageid, CameraFile * file, GPContext *context);
int sony_exif_get(Camera * camera, int imageid, CameraFile * file, GPContext *context);
int sony_mpeg_get(Camera * camera, int imageid, CameraFile * file, GPContext *context);
int sony_image_info(Camera * camera, int imageid, SonyFileType file_type, CameraFileInfo * info, GPContext *context);
int sony_file_name_get(Camera *camera, int imageid, SonyFileType file_type, char buf[13]);
int sony_is_mpeg_file_name(const char * file_name);

#endif /* SONY_H */

/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
