/* Sony DSC-F55 & MSAC-SR1 - gPhoto2 camera library
 * Copyright © 2001, 2002 Raymond Penners <raymond@dotsphinx.com>
 * Copyright © 2000 Mark Davies <mdavies@dial.pipex.com>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef SONY_H
#define SONY_H

#define SONY_CAMERA_ID "sonydscf55"
#define SONY_MODEL_MSAC_SR1 "Sony:MSAC-SR1"
#define SONY_MODEL_DCR_PC100 "Sony:DCR-PC100"
#define SONY_MODEL_TRV_20E   "Sony:TRV-20E"
#define SONY_MODEL_DSC_F55 "Sony:DSC-F55"

#define SONY_FILE_NAME_FMT "dsc%05d.jpg"

typedef struct _tagPacket {
	int valid;
	int length;
	unsigned char buffer[16384];
	unsigned char checksum;
} Packet;

struct _CameraPrivateLibrary {
	unsigned short int sequence_id;
	int msac_sr1;
};

int sony_init(Camera * camera, int ismsac);
int sony_exit(Camera * camera);
int sony_image_count(Camera * camera);
int sony_mpeg_count(Camera * camera);
int sony_image_get(Camera * camera, int imageid, CameraFile * file, GPContext *context);
int sony_thumbnail_get(Camera * camera, int imageid, CameraFile * file, GPContext *context);
int sony_exif_get(Camera * camera, int imageid, CameraFile * file, GPContext *context);
int sony_image_info(Camera * camera, int imageid, CameraFileInfo * info, GPContext *context);

#endif				/* SONY_H */

/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
